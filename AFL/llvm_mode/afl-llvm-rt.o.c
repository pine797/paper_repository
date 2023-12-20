/*
  Copyright 2015 Google LLC All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/*
   american fuzzy lop - LLVM instrumentation bootstrap
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres.

   This code is the rewrite of afl-as.h's main_payload.
*/

#include "../android-ashmem.h"
#include "../config.h"
#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>

/* This is a somewhat ugly hack for the experimental 'trace-pc-guard' mode.
   Basically, we need to make sure that the forkserver is initialized after
   the LLVM-generated runtime initialization pass, not before. */

#ifdef USE_TRACE_PC
#  define CONST_PRIO 5
#else
#  define CONST_PRIO 0
#endif /* ^USE_TRACE_PC */

/* Globals needed by the injected instrumentation. The __afl_area_initial region
   is used for instrumentation output before __afl_map_shm() has a chance to run.
   It will end up as .comm, so it shouldn't be too wasteful. */

u8  __afl_area_initial[MAP_SIZE];
u8* __afl_area_ptr = __afl_area_initial;

/*扩展: 记录关键变量值变化的MAP */
u8  __afl_sensitive_initial[SENSITIVE_MAP_SIZE];
u8* __afl_sensitive_ptr = __afl_sensitive_initial;

shared_data_t* shared_data = NULL;


__thread u32 __afl_prev_loc;

/* Running in persistent mode? */

static u8 is_persistent;

//copy ijon
uint64_t ijon_simple_hash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}
uint32_t ijon_hashint(uint32_t old, uint32_t val){
  uint64_t input = (((uint64_t)(old))<<32) | ((uint64_t)(val));
  return (uint32_t)(ijon_simple_hash(input));
}
uint32_t ijon_hashmem(uint32_t old, char* val, size_t len){
  old = ijon_hashint(old,len);
  for(size_t i = 0; i < len ; i++){
    old = ijon_hashint(old, val[i]);
  }
  return old;
}
uint32_t ijon_hashstr(uint32_t old, char* val){
  return ijon_hashmem(old, val, strlen(val));
}

//==========
SingleConstraint singleStateArray[singleStateArraySize];
SingleConstraint* __singleStateArray_ptr = singleStateArray;

AssistVar stateMachineAssistVar; //用于状态机变量监控的辅助结构
AssistVar* __stateMachineArray_ptr = &stateMachineAssistVar;

AssistVar substateAssistVar;  //用于子状态edge的监控
AssistVar* __substateAssistVar_ptr = &substateAssistVar;

//标识当前的协议状态
uint64_t CurrentStateMachineState = 0;

void updatePos(int *last,int *cur){
  *last = *cur;
  *cur = (*cur + 1) % loopArraySize;
}

/**
 * @description: 状态机变量监控函数
 * @param {int8_t} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_int8(int8_t val,int pos){
  // [1] get stateval
  uint32_t stateval = (pos << 16) + val;
  // uint32_t unindex = 5; //默认从1行开始存储，第0行提供给无状态机变量的子状态使用
  // // [2] if exist unique map line 计算大状态值对应的唯一行号：1.查看Map有无结点，有则跳过2.没有则记录
  // if ((unindex = check_if_sm_node_exist(stateval)) == -1){
  //   if (SMStateCurUsePos == StateMachineNum)
  //   {
  //     assert(SMStateCurUsePos != StateMachineNum);
  //   }
  //   add_sm_node(stateval,SMStateCurUsePos); //add stateval->SMStateCurUsePos
  //   unindex = SMStateCurUsePos;
  //   SMStateCurUsePos++;
  // }
  // [3] set currentSMState
  CurrentStateMachineState = stateval;
  // [4] feedback to bitmap
  __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->cur] = stateval;  //store
  uint32_t lastval = __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->last];
  uint32_t curval = stateval;
  uint32_t addr = ijon_hashint(lastval,curval) % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update pointer
  updatePos(&__stateMachineArray_ptr->last,&__stateMachineArray_ptr->cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {int32_t} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_int32(int32_t val,int pos){
  // [1] get stateval
  uint32_t stateval = (pos << 16) + val;
  // uint32_t unindex = 5;
  // // [2] if exist unique map line 计算大状态值对应的唯一行号：1.查看Map有无结点，有则跳过2.没有则记录
  // if ((unindex = check_if_sm_node_exist(stateval)) == -1){
  //   if (SMStateCurUsePos == StateMachineNum)
  //   {
  //     assert(SMStateCurUsePos != StateMachineNum);
  //   }
  //   add_sm_node(stateval,SMStateCurUsePos); //add stateval->SMStateCurUsePos
  //   unindex = SMStateCurUsePos;
  //   SMStateCurUsePos++;
  // }
  // [3] set currentSMState
  CurrentStateMachineState = stateval;
  // [4] feedback to bitmap
  __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->cur] = stateval;  //store
  uint32_t lastval = __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->last];
  uint32_t curval = stateval;
  uint32_t addr = ijon_hashint(lastval,curval) % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update pointer
  updatePos(&__stateMachineArray_ptr->last,&__stateMachineArray_ptr->cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {int64_t} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_int64(int64_t val,int pos){
  // [1] get stateval
  uint32_t stateval = (pos << 16) + val;
  // uint32_t unindex = 5;
  // // [2] if exist unique map line 计算大状态值对应的唯一行号：1.查看Map有无结点，有则跳过2.没有则记录
  // if ((unindex = check_if_sm_node_exist(stateval)) == -1){
  //   if (SMStateCurUsePos == StateMachineNum)
  //   {
  //     assert(SMStateCurUsePos != StateMachineNum);
  //   }
  //   add_sm_node(stateval,SMStateCurUsePos); //add stateval->SMStateCurUsePos
  //   unindex = SMStateCurUsePos;
  //   SMStateCurUsePos++;
  // }
  // [3] set currentSMState
  CurrentStateMachineState = stateval;
  // [4] feedback to bitmap
  __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->cur] = stateval;  //store
  uint32_t lastval = __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->last];
  uint32_t curval = stateval;
  uint32_t addr = ijon_hashint(lastval,curval) % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update pointer
  updatePos(&__stateMachineArray_ptr->last,&__stateMachineArray_ptr->cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {char*} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_str(char* val,int pos){
  // [1] get stateval.
  uint32_t stateval = ijon_hashstr(pos,val);
  // uint32_t unindex = 5;
  // // [2] if exist unique map line 计算大状态值对应的唯一行号：1.查看Map有无结点，有则跳过2.没有则记录
  // if ((unindex = check_if_sm_node_exist(stateval)) == -1){
  //   if (SMStateCurUsePos == StateMachineNum)
  //   {
  //     assert(SMStateCurUsePos != StateMachineNum);
  //   }
  //   add_sm_node(stateval,SMStateCurUsePos); //add stateval->SMStateCurUsePos
  //   unindex = SMStateCurUsePos;
  //   SMStateCurUsePos++;
  // }
  // [3] set currentSMState
  CurrentStateMachineState = stateval;
  // [4] feedback to bitmap
  __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->cur] = stateval;  //store
  uint32_t lastval = __stateMachineArray_ptr->LoopArray[__stateMachineArray_ptr->last];
  uint32_t curval = stateval;
  uint32_t addr = ijon_hashint(lastval,curval) % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update pointer
  updatePos(&__stateMachineArray_ptr->last,&__stateMachineArray_ptr->cur);
}

/**
 * @description: 监控组合型变量(int类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline

 * @return {*}
 */
void make_sensitive_for_multistate_int_8(int8_t val,int pos){
  // [1] calculate cur substate val (with statemacine state)
  uint64_t substateval = (pos << 16) + val;
  substateval =  (CurrentStateMachineState ^ substateval);
  __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] calculate substate edge
  uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;;
  // [4] feedback substate edge to bitmap
  uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update work ptr
  updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
}

/**
 * @description: 监控组合型变量(int类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline

 * @return {*}
 */
void make_sensitive_for_multistate_int_32(int32_t val,int pos){
  // [1] calculate cur substate val (with statemacine state)
  uint64_t substateval = (pos << 16) + val;
  substateval =  (CurrentStateMachineState ^ substateval);
  __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] calculate substate edge
  uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;;
  // [4] feedback substate edge to bitmap
  uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update work ptr
  updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
}

/**
 * @description: 监控组合型变量(int64类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline
 * @return {*}
 */
void make_sensitive_for_multistate_int_64(int64_t val,int pos){
  // [1] calculate cur substate val (with statemacine state)
  uint64_t substateval = (pos << 16) + val;
  substateval =  (CurrentStateMachineState ^ substateval);
  __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] calculate substate edge
  uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;;
  // [4] feedback substate edge to bitmap
  uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update work ptr
  updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
}

/**
 * @description: 监控组合型变量(char*类型变量)

 * @param {char*} val
 * @param {int} pos
 * @return {*}
 */
void make_sensitive_for_multistate_char(char* val,int pos){
  // [1] calculate cur substate val (with statemacine state)
  uint32_t strval = ijon_hashstr(0,val) % MAX_INT32;
  uint64_t substateval =  ((pos << 16) + strval) % MAX_INT64;
  substateval = (CurrentStateMachineState ^ substateval);
  __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] calculate substate edge
  uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
  // [4] feedback substate edge to bitmap
  uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
  __afl_sensitive_ptr[addr]++;
  // [5] update work ptr
  updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
}


int findInArray(uint64_t *array,int val){
  for (size_t i = 0; i < MAX_SINGLE_STATE_FEED; i++)
  {
    if (array[i]==val)
    {
      return 1;
    }
  }
  return 0;
}
/**
 * @description: 监控单一类型的协议变量(int类型变量)


 * @return {*}
 */
void make_sensitive_for_singlestate_int_8(int8_t val,int pos){
  // [1] calculate substate val
  uint32_t substateval = (pos << 16) + val;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] check befor feedback to bitmap
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }

  if (findInArray(localAssistVar->ConstrainArray,substateval))  //非新状态值
  {
    // calculate substateval with statemachine 
    substateval = (CurrentStateMachineState ^ substateval);
    // store cur substate val
    __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
    // [4] calculate substate edge
    uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
    // [5] feedback to bitmap
    uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
    // [6] update work pointer
    updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
  }else{
    //新状态值，两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConstrainArray）
    if (IsBeyondRange==0)
    {
      // calculate substateval with statemachine 
      substateval = (CurrentStateMachineState ^ substateval);
      // store cur sub state val
      __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
      localAssistVar->ConstrainArray[localAssistVar->cur++] = substateval;
      // [4] calculate substate edge
      uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
      // [5] feedback to bitmap
      uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
      // [6] update work pointer
      updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
    }
  }
}

/**
 * @description: 监控单一类型的协议变量(int类型变量)
 * @return {*}
 */
void make_sensitive_for_singlestate_int_32(int32_t val,int pos){
  // [1] calculate substate val
  uint32_t substateval = (pos << 16) + val;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] check befor feedback to bitmap
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }

  if (findInArray(localAssistVar->ConstrainArray,substateval))  //非新状态值
  {
    // calculate substateval with statemachine 
    substateval = (CurrentStateMachineState ^ substateval);
    // store cur substate val
    __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
    // [4] calculate substate edge
    uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
    // [5] feedback to bitmap
    uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
    // [6] update work pointer
    updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
  }else{
    //新状态值，两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConstrainArray）
    if (IsBeyondRange==0)
    {
      // calculate substateval with statemachine 
      substateval = (CurrentStateMachineState ^ substateval);
      // store cur sub state val
      __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
      localAssistVar->ConstrainArray[localAssistVar->cur++] = substateval;
      // [4] calculate substate edge
      uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
      // [5] feedback to bitmap
      uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
      // [6] update work pointer
      updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
    }
  }
}

void make_sensitive_for_singlestate_int_64(int64_t val,int pos){
  // [1] calculate substate val
  uint32_t substateval = (pos << 16) + val;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] check befor feedback to bitmap
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }

  if (findInArray(localAssistVar->ConstrainArray,substateval))  //非新状态值
  {
    // calculate substateval with statemachine 
    substateval = (CurrentStateMachineState ^ substateval);
    // store cur substate val
    __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
    // [4] calculate substate edge
    uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
    // [5] feedback to bitmap
    uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
    // [6] update work pointer
    updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
  }else{
    //新状态值，两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConstrainArray）
    if (IsBeyondRange==0)
    {
      // calculate substateval with statemachine 
      substateval = (CurrentStateMachineState ^ substateval);
      // store cur sub state val
      __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
      localAssistVar->ConstrainArray[localAssistVar->cur++] = substateval;
      // [4] calculate substate edge
      uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
      // [5] feedback to bitmap
      uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
      // [6] update work pointer
      updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
    }
  }
}

/**
 * @description: 监控单一类型的协议变量(char*类型变量)
 * @param {char*} val
 * @param {int} pos
 * @return {*}
 */
void make_sensitive_for_singlestate_char(char* val,int pos){
  // [1] calculate substate val
  uint32_t strval = ijon_hashstr(0,val) % MAX_INT32;
  uint64_t substateval = ((pos << 16) + strval) % MAX_INT64;
  // [2] get last substate val
  uint64_t lastsubstateval = __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->last];
  // [3] check befor feedback to bitmap
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }

  if (findInArray(localAssistVar->ConstrainArray,substateval))  //非新状态值
  {
    // calculate substateval with statemachine 
    substateval = (CurrentStateMachineState ^ substateval);
    // store cur substate val
    __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
    // [4] calculate substate edge
    uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
    // [5] feedback to bitmap
    uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
    // [6] update work pointer
    updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
  }else{
    //新状态值，两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConstrainArray）
    if (IsBeyondRange==0)
    {
      // calculate substateval with statemachine 
      substateval = (CurrentStateMachineState ^ substateval);
      // store cur sub state val
      __substateAssistVar_ptr->LoopArray[__substateAssistVar_ptr->cur] = substateval;
      localAssistVar->ConstrainArray[localAssistVar->cur++] = substateval;
      // [4] calculate substate edge
      uint32_t substate_edge = ijon_hashint(lastsubstateval,substateval) % MAX_INT32;
      // [5] feedback to bitmap
      uint32_t addr = substate_edge % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
      // [6] update work pointer
      updatePos(&__substateAssistVar_ptr->last,&__substateAssistVar_ptr->cur);
    }
  }
}


/* SHM setup. */

static void __afl_map_shm(void) {

  u8 *id_str = getenv(SHM_ENV_VAR);

  /* If we're running under AFL, attach to the appropriate region, replacing the
     early-stage __afl_area_initial region that is needed to allow some really
     hacky .init code to work correctly in projects such as OpenSSL. */

  if (id_str) {

    u32 shm_id = atoi(id_str);

    shared_data = shmat(shm_id, NULL, 0);

    if (shared_data == (void *)-1) _exit(1);

    __afl_area_ptr = &shared_data->afl_area[0];

    __afl_sensitive_ptr = &shared_data->afl_sensitive_area[0];

    __singleStateArray_ptr = &shared_data->singleStateArray[0];


    /* Whooooops. */

    // if (__afl_area_ptr == (void *)-1) _exit(1);

    /* Write something into the bitmap so that even with low AFL_INST_RATIO,
       our parent doesn't give up on us. */

    __afl_area_ptr[0] = 1;

  }

}

/* Fork server logic. */

static void __afl_start_forkserver(void) {

  static u8 tmp[4];
  s32 child_pid;

  u8  child_stopped = 0;

  /* Phone home and tell the parent that we're OK. If parent isn't there,
     assume we're not running in forkserver mode and just execute program. */

  if (write(FORKSRV_FD + 1, tmp, 4) != 4) return;

  while (1) {

    u32 was_killed;
    int status;

    /* Wait for parent by reading from the pipe. Abort if read fails. */

    if (read(FORKSRV_FD, &was_killed, 4) != 4) _exit(1);

    /* If we stopped the child in persistent mode, but there was a race
       condition and afl-fuzz already issued SIGKILL, write off the old
       process. */

    if (child_stopped && was_killed) {
      child_stopped = 0;
      if (waitpid(child_pid, &status, 0) < 0) _exit(1);
    }

    if (!child_stopped) {

      /* Once woken up, create a clone of our process. */

      child_pid = fork();
      if (child_pid < 0) _exit(1);

      /* In child process: close fds, resume execution. */

      if (!child_pid) {

        close(FORKSRV_FD);
        close(FORKSRV_FD + 1);
        return;
  
      }

    } else {

      /* Special handling for persistent mode: if the child is alive but
         currently stopped, simply restart it with SIGCONT. */

      kill(child_pid, SIGCONT);
      child_stopped = 0;

    }

    /* In parent process: write PID to pipe, then wait for child. */

    if (write(FORKSRV_FD + 1, &child_pid, 4) != 4) _exit(1);

    if (waitpid(child_pid, &status, is_persistent ? WUNTRACED : 0) < 0)
      _exit(1);

    /* In persistent mode, the child stops itself with SIGSTOP to indicate
       a successful run. In this case, we want to wake it up without forking
       again. */

    if (WIFSTOPPED(status)) child_stopped = 1;

    /* Relay wait status to pipe, then loop back. */

    if (write(FORKSRV_FD + 1, &status, 4) != 4) _exit(1);

  }

}

/* A simplified persistent mode handler, used as explained in README.llvm. */

int __afl_persistent_loop(unsigned int max_cnt) {

  static u8  first_pass = 1;
  static u32 cycle_cnt;

  if (first_pass) {

    /* Make sure that every iteration of __AFL_LOOP() starts with a clean slate.
       On subsequent calls, the parent will take care of that, but on the first
       iteration, it's our job to erase any trace of whatever happened
       before the loop. */

    if (is_persistent) {

      memset(__afl_area_ptr, 0, MAP_SIZE);
      __afl_area_ptr[0] = 1;
      __afl_prev_loc = 0;
      // add by sxp
      memset(__afl_sensitive_ptr,0,SENSITIVE_MAP_SIZE); 
      memset(__stateMachineArray_ptr,0,sizeof(stateMachineAssistVar));
      memset(__substateAssistVar_ptr,0,sizeof(substateAssistVar));

      CurrentStateMachineState = 0;
    }

    cycle_cnt  = max_cnt;
    first_pass = 0;
    return 1;

  }

  if (is_persistent) {

    if (--cycle_cnt) {

      raise(SIGSTOP);

      __afl_area_ptr[0] = 1;
      __afl_prev_loc = 0;

      memset(__stateMachineArray_ptr,0,sizeof(stateMachineAssistVar));
      memset(__substateAssistVar_ptr,0,sizeof(substateAssistVar));

      CurrentStateMachineState = 0;

      return 1;

    } else {

      /* When exiting __AFL_LOOP(), make sure that the subsequent code that
         follows the loop is not traced. We do that by pivoting back to the
         dummy output region. */

      __afl_area_ptr = __afl_area_initial;
      // add by sxp
      __afl_sensitive_ptr = __afl_sensitive_initial;

    }

  }

  return 0;

}


/* This one can be called from user code when deferred forkserver mode
    is enabled. */

void __afl_manual_init(void) {

  static u8 init_done;

  if (!init_done) {

    __afl_map_shm();
    __afl_start_forkserver();
    init_done = 1;

  }

}


/* Proper initialization routine. */

__attribute__((constructor(CONST_PRIO))) void __afl_auto_init(void) {

  is_persistent = !!getenv(PERSIST_ENV_VAR);

  if (getenv(DEFER_ENV_VAR)) return;

  __afl_manual_init();

}


/* The following stuff deals with supporting -fsanitize-coverage=trace-pc-guard.
   It remains non-operational in the traditional, plugin-backed LLVM mode.
   For more info about 'trace-pc-guard', see README.llvm.

   The first function (__sanitizer_cov_trace_pc_guard) is called back on every
   edge (as opposed to every basic block). */

void __sanitizer_cov_trace_pc_guard(uint32_t* guard) {
  __afl_area_ptr[*guard]++;
}


/* Init callback. Populates instrumentation IDs. Note that we're using
   ID of 0 as a special value to indicate non-instrumented bits. That may
   still touch the bitmap, but in a fairly harmless way. */

void __sanitizer_cov_trace_pc_guard_init(uint32_t* start, uint32_t* stop) {

  u32 inst_ratio = 100;
  u8* x;

  if (start == stop || *start) return;

  x = getenv("AFL_INST_RATIO");
  if (x) inst_ratio = atoi(x);

  if (!inst_ratio || inst_ratio > 100) {
    fprintf(stderr, "[-] ERROR: Invalid AFL_INST_RATIO (must be 1-100).\n");
    abort();
  }

  /* Make sure that the first element in the range is always set - we use that
     to avoid duplicate calls (which can happen as an artifact of the underlying
     implementation in LLVM). */

  *(start++) = R(MAP_SIZE - 1) + 1;

  while (start < stop) {

    if (R(100) < inst_ratio) *start = R(MAP_SIZE - 1) + 1;
    else *start = 0;

    start++;

  }

}
