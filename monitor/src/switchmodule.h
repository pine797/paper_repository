#ifndef _SWITCH_MODULE_H_
#define _SWITCH_MODULE_H_

#include "../include/cppheader.h"
#include "../include/llvmheader.h"

using namespace std;
using namespace llvm;

typedef struct SwitchCase{
    BasicBlock *beginBB;
    BasicBlock *endBB;
    int BBnum;
}SwitchCase;

typedef struct SwitchInstrumentObj{
    SwitchInst *swinst;
    Instruction *swcondition;
    vector<SwitchCase> swCase;
    string semantics;
}SwitchCaseInstrumentInfo;


/**
 * @description: 提取程序中Switch/IF类型关键变量
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,
 * @param vector<vector<string>> MeaningDict,
 * @param vector<Instruction*>} NormalVariableVec
 * @return {*}
 */
void handleSwitch(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> &MeaningDict);

void handleNormalSwitch(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict);
/**
 * @description: 提取Swtich的所有Case,获取各Case的起始和结尾基本块,以及Case所在的基本块个数
 * @param {Instruction} *instruction
 * @param {vector<SwitchCase>} &swCase
 * @return {*}
 */
void extractAllCase(Instruction *instruction,vector<SwitchCase> &swCase,int &error);

/**
 * @description: 提取Switch condition的语义名称
 * @param {Instruction} *Inst
 * @return {*}
 */
string extractSwitchSemantics(Instruction *Inst);
/**
 * @description: 获取Switch的末尾块，如sw.epilog，没有则以default
 * @param {SwitchInst} *swInst
 * @return {*}
 */
BasicBlock* getSwitchEnd(SwitchInst *swInst,BasicBlock *basicblock);
/**
 * @description: 统计Case的BasicBlock个数
 * @param {BasicBlock} *start
 * @param {BasicBlock} *end
 * @return {*}
 */
int calculateCaseBBSize(BasicBlock *start,BasicBlock *end);
/**
 * @description: 检查调用函数的前后缀的次数是否符合80%的size
 * @param {int} swCaseSize
 * @param {map<StringRef,int> CountMap,map<StringRef,int>} functionCountMap
 * @return {*}
 */
bool checkSwitchCaseCallTimes(int swCaseSize,map<StringRef,int> CountMap,map<StringRef,int> functionCountMap);
/**
 * @description: 统计Switch代码的整体复杂度
 * @param {vector<SwitchCase>} swCase
 * @return {*}
 */
int countSwCodeComplexity(vector<SwitchCase> swCase);
/**
 * @description: 检查Switch各Case的特征，如复杂度、调用函数名称的前后缀
 * @param {vector<SwitchCase>} swCase
 * @return {*}
 */
bool checkCaseFeatures(vector<SwitchCase> swCase);

bool cmp(pair<StringRef,int>&a,pair<StringRef,int>&b);

bool cmp_int_int(pair<int,int>&a,pair<int,int>&b);

/**
 * @description: 处理候选的Switch目标
 * @param {vector<SwitchInstrumentObj> switchResults,
 * @param vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateSwitchObj(vector<SwitchInstrumentObj> switchResults,vector<vector<string>> MeaningDict);
/**
 * @description: 处理候选的Switch状态变量
 * @param {vector<SwitchInstrumentObj> switchResults,
 * @param vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateStateMachineSwitch(vector<SwitchInstrumentObj> switchResults);

typedef struct IfConditionObj
{

    vector<BasicBlock*> conditionBBVec; //If条件表达式中所有的BasicBlock
    BasicBlock *branchTrueBeginBB,*branchTrueEndBB; //记录条件表达式为true所包裹的代码块[... , ...)
    int flags;
    int IfObjNumber;    //独立的一个IF对象的标识，因为有些是if( || )，这种在LLVM IR中基本块后继不是两者均以if开头，所以引入这个变量成员
}IfConditionObj;

typedef struct ArgStruct{
    Value *argval;  //函数参数Value*
    Type* argtype;  //函数参数类型
    StringRef operandName;  //参数名称(如果为空，则取操作数的名称)
}ArgStruct;

typedef struct IfTemplate{
    Function *function; //调用函数名称
    ICmpInst *icmpInst; //Icmp指令
    vector<ArgStruct> Args;    //函数参数的信息

}IfTemplate;

void storeTheIfObj(vector<IfConditionObj> &AllIfObjInCurFunc,vector<BasicBlock*> resultVec,vector<BasicBlock*> rangeBB,int IfObjNumber);

bool checkIsIF(BasicBlock *basicblock);

void handleIfElse(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict);

int checkSuccStartWithIfNumber(BasicBlock *basicblock);

BasicBlock* getNotStartWithIfBasicBlock(BasicBlock *basicblock);

void traverseForIFCondition(BasicBlock *basicblock,vector<BasicBlock*> &resultVec);

void extractBranchTrue(BasicBlock *curBB,vector<BasicBlock*> &rangeBB);

bool checkIsTargetIFObj(IfConditionObj ifConditionObj,vector<IfTemplate> &temVec,vector<vector<string>> MeaningDict);

void analysisAllIfObjInCurFunc(vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict);

bool checkArgsIfConstr(Value *args);

Value* getArgsFromStrCmpFunction(CallInst *callInst);

bool checkIsTargetIFObjEqualTemplate(IfConditionObj ifConditionObj,vector<IfTemplate> temVec);

// void convertMaptoVec(map<int,int> countMap,vector<pair<int,int>> &vec);
template<typename type1,typename type2>
void convertMaptoVec(map<type1,type2> countMap,vector<pair<type1,type2>> &vec);

int calIfComplexity(BasicBlock *beginBB,BasicBlock *endBB);

// void handleCandidateIfObj(vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue);
void handleCandidateIfObj(vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue,vector<vector<string>> MeaningDict);

bool iterateIFCodeForCallFeature(vector<IfConditionObj> AllIfObjInCurFunc,int flags,int length,vector<int> &candidateResVec);

bool checkFuncArgsIfRelateProtocol(CallInst *callInst,vector<vector<string>> MeaningDict);

void handleMultiSingleIFObj(vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict,int flags,int isMulti);

bool checkIFObjArgsRelateProtocol(vector<IfConditionObj> candidateIFObjVec,vector<vector<string>> MeaningDict);

void extractFuncArgsUsed(CallInst *callInst,map<StringRef,int> &CountArgsMap);

template<typename type1,typename type2>
bool cmp2(pair<type1,type2>&a,pair<type1,type2>&b);

BasicBlock* getTheTureBasicBlockofIF(BasicBlock *basicblock);

bool insertFunctionInOneCondition(IfConditionObj obj,vector<vector<string>> MeanDict,StringRef semantics,int isMulti);

void insertFunctionForIF(vector<IfConditionObj> AllIfObjInCurFunc,int flags,vector<vector<string>> MeanDict,StringRef semantics,int isMulti);

StringRef extractFunctionArgSemantics(Value *argval);

bool checkIfStrCmpFunction(CallInst *callinst,Function *calledFunction);

void extractIFObjInCurFunction(Function *function,vector<IfConditionObj> &AllIfObjInCurFunc);

void handleCandidateSwitchObj_NotStateCondition(vector<SwitchInstrumentObj> switchResults);

void extractSwCaseValue(SwitchInst *swInst,vector<int> &SwCaseValueVec);

int checkSingleIfCodeComplexity(IfConditionObj ifObjPtr,int complexity);
#endif
