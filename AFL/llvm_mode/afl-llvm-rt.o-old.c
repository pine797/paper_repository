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
uint64_t simple_number_hash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}


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
AssistVar multiStateArray[multiStateArraySize];
SingleConstraint singleStateArray[singleStateArraySize];

AssistVar* __multiStateArray_ptr = multiStateArray;
SingleConstraint* __singleStateArray_ptr = singleStateArray;

AssistVar stateMachineAssistVar; //用于状态机变量监控的辅助结构

AssistVar* __stateMachineArray_ptr = &stateMachineAssistVar;

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
  // [1] val+pos -> unique value
  uint32_t stateval = ijon_hashint(pos,val);

  stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur] = stateval;

  if (stateMachineAssistVar.FirstUseFlag == 0)
  {
    stateMachineAssistVar.FirstUseFlag = 1;
    uint32_t addr = stateval % SENSITIVE_MAP_SIZE;
    CurrentStateMachineState = stateval;
    __afl_sensitive_ptr[addr]++;
  }else{
    uint32_t lastval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.last];
    uint32_t curval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur];
    uint32_t nowval = ijon_hashint(lastval,curval);
    CurrentStateMachineState = nowval;
    uint32_t addr = nowval % SENSITIVE_MAP_SIZE;

    __afl_sensitive_ptr[addr]++;
  }
  // update work pointer
  updatePos(&stateMachineAssistVar.last,&stateMachineAssistVar.cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {int32_t} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_int32(int32_t val,int pos){
  // [1] val+pos -> unique value
  uint32_t stateval = ijon_hashint(pos,val);
  
  stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur] = stateval;

  if (stateMachineAssistVar.FirstUseFlag == 0)
  {
    stateMachineAssistVar.FirstUseFlag = 1;
    uint32_t addr = stateval % SENSITIVE_MAP_SIZE;
    CurrentStateMachineState = stateval;

    __afl_sensitive_ptr[addr]++;
  }else{
    uint32_t lastval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.last];
    uint32_t curval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur];
    uint32_t nowval = ijon_hashint(lastval,curval);
    CurrentStateMachineState = nowval;
    uint32_t addr = nowval % SENSITIVE_MAP_SIZE;

    __afl_sensitive_ptr[addr]++;
  }
  // update work pointer
  updatePos(&stateMachineAssistVar.last,&stateMachineAssistVar.cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {int64_t} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_int64(int64_t val,int pos){
  // [1] val+pos -> unique value
  uint32_t stateval = ijon_hashint(pos,val);
  
  stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur] = stateval;

  if (stateMachineAssistVar.FirstUseFlag == 0)
  {
    stateMachineAssistVar.FirstUseFlag = 1;
    uint32_t addr = stateval % SENSITIVE_MAP_SIZE;
    CurrentStateMachineState = stateval;

    __afl_sensitive_ptr[addr]++;
  }else{
    uint32_t lastval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.last];
    uint32_t curval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur];
    uint32_t nowval = ijon_hashint(lastval,curval);
    CurrentStateMachineState = nowval;
    uint32_t addr = nowval % SENSITIVE_MAP_SIZE;

    __afl_sensitive_ptr[addr]++;
  }
  // update work pointer
  updatePos(&stateMachineAssistVar.last,&stateMachineAssistVar.cur);
}
/**
 * @description: 状态机变量监控函数
 * @param {char*} val
 * @param {int} pos
 * @return {*}
 */
void stateMachineVarMonitor_str(char* val,int pos){
  uint32_t stateval = ijon_hashstr(pos,val);
  stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur] = stateval;

  if (stateMachineAssistVar.FirstUseFlag == 0)
  {
    stateMachineAssistVar.FirstUseFlag = 1;
    uint32_t addr = stateval % SENSITIVE_MAP_SIZE;
    CurrentStateMachineState = stateval;
    __afl_sensitive_ptr[addr]++;
  }else{
    uint32_t lastval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.last];
    uint32_t curval = stateMachineAssistVar.LoopArray[stateMachineAssistVar.cur];
    uint32_t nowval = ijon_hashint(lastval,curval);
    CurrentStateMachineState = nowval;

    uint32_t addr = ijon_hashint(lastval,curval) % SENSITIVE_MAP_SIZE;

    __afl_sensitive_ptr[addr]++;
  }
  updatePos(&stateMachineAssistVar.last,&stateMachineAssistVar.cur);
}


/**
 * @description: 监控组合型变量(int类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline

 * @return {*}
 */
void make_sensitive_for_multistate_int_8(int8_t val,int pos){
  AssistVar *localAssistVar = &__multiStateArray_ptr[pos-singleStateArraySize]; //插桩时pos有4096 4097...但这些局部的数据结构是从0下标开始，所以要-

  //记录当前command至cur中
  localAssistVar->LoopArray[localAssistVar->cur] = simple_number_hash(val);

  //first used -> only one state -> feedback
  if (localAssistVar->FirstUseFlag == 0)
  {
    localAssistVar->FirstUseFlag = 1;
    uint32_t addr = ijon_hashint(pos,val);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }else{
    // start feedback
    uint32_t lastval = localAssistVar->LoopArray[localAssistVar->last];
    uint32_t curval = localAssistVar->LoopArray[localAssistVar->cur];
    // get hashvalue
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t stateval = ijon_hashint(lastval,curval);
    uint32_t prefix = pos;
    uint32_t addr = ijon_hashint(prefix,stateval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;

    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }
  //更新工作指针
  updatePos(&localAssistVar->last,&localAssistVar->cur);
}

/**
 * @description: 监控组合型变量(int类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline

 * @return {*}
 */
void make_sensitive_for_multistate_int_32(int32_t val,int pos){
  AssistVar *localAssistVar = &__multiStateArray_ptr[pos-singleStateArraySize];

  //记录当前command至cur中
  localAssistVar->LoopArray[localAssistVar->cur] = simple_number_hash(val);

  //first used -> only one state -> feedback
  if (localAssistVar->FirstUseFlag == 0)
  {
    localAssistVar->FirstUseFlag = 1;
    uint32_t addr = ijon_hashint(pos,val);
    
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }else{
    // start feedback
    uint32_t lastval = localAssistVar->LoopArray[localAssistVar->last];
    uint32_t curval = localAssistVar->LoopArray[localAssistVar->cur];
    // get hashvalue
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t stateval = ijon_hashint(lastval,curval);
    uint32_t addr = ijon_hashint(prefix,stateval);

    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }
  //更新工作指针
  updatePos(&localAssistVar->last,&localAssistVar->cur);
}

/**
 * @description: 监控组合型变量(int64类型变量)
 * @param {char} *filename 文件名称
 * @param {int} codeline
 * @return {*}
 */
void make_sensitive_for_multistate_int_64(int64_t val,int pos){
  AssistVar *localAssistVar = &__multiStateArray_ptr[pos-singleStateArraySize];

  //记录当前command至cur中
  localAssistVar->LoopArray[localAssistVar->cur] = simple_number_hash(val);

  //first used -> only one state -> feedback
  if (localAssistVar->FirstUseFlag == 0)
  {
    localAssistVar->FirstUseFlag = 1;
    uint32_t addr = ijon_hashint(pos,val);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;

    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }else{
    // start feedback
    uint32_t lastval = localAssistVar->LoopArray[localAssistVar->last];
    uint32_t curval = localAssistVar->LoopArray[localAssistVar->cur];
    // get hashvalue
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t stateval = ijon_hashint(lastval,curval);
    uint32_t addr = ijon_hashint(prefix,stateval);

    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }
  //更新工作指针
  updatePos(&localAssistVar->last,&localAssistVar->cur);
}

/**
 * @description: 监控组合型变量(char*类型变量)

 * @param {char*} val
 * @param {int} pos
 * @return {*}
 */
void make_sensitive_for_multistate_char(char* val,int pos){
  AssistVar *localAssistVar = &__multiStateArray_ptr[pos-singleStateArraySize];

  //记录当前command至cur中
  int intval = ijon_hashstr(0,val);
  localAssistVar->LoopArray[localAssistVar->cur] = intval;

  //first used -> only one state -> feedback
  if (localAssistVar->FirstUseFlag == 0)
  {
    localAssistVar->FirstUseFlag = 1;
    uint32_t addr = ijon_hashint(pos,intval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }else{
    // start feedback
    uint32_t lastval = localAssistVar->LoopArray[localAssistVar->last];
    uint32_t curval = localAssistVar->LoopArray[localAssistVar->cur];
    // get hashvalue
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t stateval = ijon_hashint(lastval,curval);
    uint32_t addr = ijon_hashint(prefix,stateval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    //mark in the shared memory
    __afl_sensitive_ptr[addr]++;
  }
  //更新工作指针
  updatePos(&localAssistVar->last,&localAssistVar->cur);
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

  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  if (localAssistVar->disableFlags)
  {
    return;
  }
  
  // FILE *fp = fopen("/tmp/debuglog.txt","a");
  // fprintf(fp,"pos:%d\tfeedback length:%d\n",pos,localAssistVar->cur);
  // fclose(fp);

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }
  uint32_t hashval = ijon_simple_hash(val);
  if (findInArray(localAssistVar->ConstrainArray,hashval))
  {
    //找到就应该反馈
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t addr = ijon_hashint(prefix,hashval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
  }else{
    //找不到，也有两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConArray）
    if (IsBeyondRange==0)
    {
      localAssistVar->ConstrainArray[localAssistVar->cur++] = hashval;
      // uint32_t prefix = ijon_hashstr(codeline,filename);
      uint32_t prefix = pos;
      uint32_t addr = ijon_hashint(prefix,hashval);
      addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
    }else{
      localAssistVar->disableFlags = 1;
    }
  }
}

/**
 * @description: 监控单一类型的协议变量(int类型变量)


 * @return {*}
 */
void make_sensitive_for_singlestate_int_32(int32_t val,int pos){
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  if (localAssistVar->disableFlags)
  {
    return;
  }

  // FILE *fp = fopen("/tmp/debuglog.txt","a");
  // fprintf(fp,"[32]pos:%d\tfeedback length:%d\n",pos,localAssistVar->cur);
  // fclose(fp);

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }
  uint32_t hashval = ijon_simple_hash(val);
  if (findInArray(localAssistVar->ConstrainArray,hashval))
  {
    //找到就应该反馈
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t addr = ijon_hashint(prefix,hashval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
  }else{
    //找不到，也有两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConArray）
    if (IsBeyondRange==0)
    {
      localAssistVar->ConstrainArray[localAssistVar->cur++] = hashval;
      // uint32_t prefix = ijon_hashstr(codeline,filename);
      uint32_t prefix = pos;
      uint32_t addr = ijon_hashint(prefix,hashval);
      addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
    }else{
      localAssistVar->disableFlags = 1;
    }
  }
}

void make_sensitive_for_singlestate_int_64(int64_t val,int pos){
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  if (localAssistVar->disableFlags)
  {
    return;
  }
  // FILE *fp = fopen("/tmp/debuglog.txt","a");
  // fprintf(fp,"[64]pos:%d\tfeedback length:%d\n",pos,localAssistVar->cur);
  // fclose(fp);

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }
  uint32_t hashval = ijon_simple_hash(val);
  if (findInArray(localAssistVar->ConstrainArray,hashval))
  {
    //找到就应该反馈
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t addr = ijon_hashint(prefix,hashval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
  }else{
    //找不到，也有两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConArray）
    if (IsBeyondRange==0)
    {
      localAssistVar->ConstrainArray[localAssistVar->cur++] = hashval;
      // uint32_t prefix = ijon_hashstr(codeline,filename);
      uint32_t prefix = pos;
      uint32_t addr = ijon_hashint(prefix,hashval);
      addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
    }else{
      localAssistVar->disableFlags = 1;
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
  SingleConstraint *localAssistVar = &__singleStateArray_ptr[pos];

  if (localAssistVar->disableFlags)
  {
    return;
  }

  uint8_t IsBeyondRange = 0;
  if (localAssistVar->cur >= MAX_SINGLE_STATE_FEED)
  {
    IsBeyondRange = 1;
  }
  uint32_t hashval = ijon_hashstr(pos,val);
  if (findInArray(localAssistVar->ConstrainArray,hashval))
  {
    //找到就应该反馈
    // uint32_t prefix = ijon_hashstr(codeline,filename);
    uint32_t prefix = pos;
    uint32_t addr = ijon_hashint(prefix,hashval);
    addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
    __afl_sensitive_ptr[addr]++;
  }else{
    //找不到，也有两种情景：1.越界（不反馈）和2.未越界（反馈+添加val至ConArray）
    if (IsBeyondRange==0)
    {
      localAssistVar->ConstrainArray[localAssistVar->cur++] = hashval;
      // uint32_t prefix = ijon_hashstr(codeline,filename);
      uint32_t prefix = pos;
      uint32_t addr = ijon_hashint(prefix,hashval);
      addr = (CurrentStateMachineState ^ addr) % SENSITIVE_MAP_SIZE;
      __afl_sensitive_ptr[addr]++;
    }else{
      localAssistVar->disableFlags = 1;
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

    // __multiStateArray_ptr = &shared_data->multiStateArray[0];
    __singleStateArray_ptr = &shared_data->singleStateArray[0];
    
    // __afl_area_ptr = shmat(shm_id, NULL, 0);

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
      memset(__multiStateArray_ptr,0,multiStateArraySize);  //组合类型的这个数据结构，是用于统计前后状态的，理应清空吧? 
      // memset(__singleStateArray_ptr,0,singleStateArraySize);//FUZZ期间是否应该清空__singleStateArray_ptr? 这是用来限制单一状态变量反馈的个数的，如果清空了，则可能会保存新的值，即限制措施失效；似乎看起来是不应该清空的
      memset(__stateMachineArray_ptr,0,sizeof(stateMachineAssistVar));
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

      memset(__multiStateArray_ptr,0,multiStateArraySize);  //AFL_LOOP每轮Fuzz完毕后应清空  //注释
      memset(__stateMachineArray_ptr,0,sizeof(stateMachineAssistVar));
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
