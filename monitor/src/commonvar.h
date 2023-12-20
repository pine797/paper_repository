#include "../include/cppheader.h"
#include "../include/llvmheader.h"
#include "../include/switchmodule.h"


using namespace std;
using namespace llvm;



int findInstFromWhichIFObj(vector<IfConditionObj> AllIfObjInCurFunc,Instruction* instruction);

// void countAllStructureUsing(Module &M,map<Function*,int> CallGraphMap);
void countAllStructureUsing(Module &M);

void backtraceToGetStructureType(Instruction *instruction,map<Type*,int> &resultVec);

void backtraceToGetStructureType_onelevel(BasicBlock *basicblock,vector<Instruction*> &tmp,queue<vector<Instruction*>> &Iqueue,map<Type*,int> &resultVec);
/**
 * @description: 检查instruction是否是IF的条件表达式
 * @param {Instruction} *instruction
 * @return {*}
 */
bool checkInstructionIfIF(Instruction *instruction,vector<IfConditionObj> AllIfObjInCurFunc);

StringRef backtraceForMeaning(Instruction *instruction,BasicBlock *constrBasicblock);
/**
 * @description: 遍历Module所有Load指令，重点关注位于IF和Switch条件表达式的Load，扫描后逐一处理
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,vector<vector<string>>} MeaningDict,vector<Instruction*> NormalVariableVec
 * @return {*}
 */
void iterateAllSwAndIfCondition(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict);
/**
 * @description: 处理LLVM IR的变量名称，删除末尾的数字
 * @param {StringRef} str
 * @return {*}
 */
StringRef processVariableNameWithoutNum(StringRef str);

bool checkIfValueFromArgs(Function *function,Instruction *instruction);
/**
 * @description: 处理Function内的候选对象
 * @param {Function} *function
 * @param {map<StringRef,vector<Instruction*>>} candidateResultsMap
 * @return {*}
 */
void handleCandidateResult(Function *function,map<string,vector<Instruction*>> candidateResultsMap,vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict);
/**
 * @description: 检查目标Instruction是否位于ICMP的条件表达式中，从sourceInst（条件表达式）开始遍历其操作数，向上回溯，匹配是否==instruction
 * @param {BasicBlock} *curBasicBlock
 * @param {Instruction} *instruction
 * @param {Instruction} *sourceInst
 * @return {*}
 */
bool checkInstructionIfIcmpCondition(BasicBlock *curBasicBlock,Instruction *instruction,Instruction *icmpInst);

bool checkBBSuccessorIfStartWithTargetStringref(BasicBlock *curbasicblock,StringRef targetString);
/**
 * @description: 检查指令是否存在于函数
 * @param {Instruction} *instruction
 * @return {*}
 */
bool checkInstructionIfSwitch(Instruction *instruction,SwitchInst **res_switchInst);
/**
 * @description: 检查参数args在function->begin() ~ endInst中是否以任意形式被使用过
 * @param {Function} *function
 * @param {Instruction} *endInst
 * @param {Argument} *args
 * @param {StoreInst} **finalStoreInst : 返回对函数参数赋值的store指令
 * @return {*}
 */
bool checkArgsOnlyUseOnce(Function *function,Instruction *endInst,Argument *args,StoreInst **finalStoreInst);
/**
 * @description: 检查instruction是否来源于函数的某个参数
 * @param {Function} *function
 * @param {Instruction} *instruction
 * @param {int} &ArgPos
 * @return {*}
 */
bool checkIfInstFromArgs(Function *function,Instruction *instruction,int &ArgPos);

Instruction* determineWhitchToMonitor(Instruction *instruction);

bool checkIcmpInstFromTargetPoint(Instruction *targetPoint);