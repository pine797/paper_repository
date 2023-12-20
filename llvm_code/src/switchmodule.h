#ifndef _SWITCH_MODULE_H_
#define _SWITCH_MODULE_H_

#include "../include/cppheader.h"
#include "../include/llvmheader.h"
#include "globalvar.h"

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



void handleBranch_Candidate_Mealy(Module &M,map<Function*,int> CallGraphMap);

void handleStateVarInSwitchScene(Module &M,map<Function*,int> CallGraphMap,vector<string> MeaningDict,vector<StateVar> MealyVarVec);


void extractAllCase(Instruction *instruction,vector<SwitchCase> &swCase,int &error);


string extractSwitchSemantics(Instruction *Inst);

BasicBlock* getSwitchEnd(SwitchInst *swInst,BasicBlock *basicblock);

int calculateCaseBBSize(BasicBlock *start,BasicBlock *end);

bool checkSwitchCaseCallTimes(int swCaseSize,map<StringRef,int> CountMap,map<StringRef,int> functionCountMap);

int countSwCodeComplexity(vector<SwitchCase> swCase);

bool checkCaseFeatures(vector<SwitchCase> swCase);

bool cmp(pair<StringRef,int>&a,pair<StringRef,int>&b);

bool cmp_int_int(pair<int,int>&a,pair<int,int>&b);


void handleCandidateSubVarInSwitchScene(vector<SwitchInstrumentObj> switchResults);

void handleCandidateMealyVarInSwitchScene(vector<SwitchInstrumentObj> switchResults);

typedef struct IfConditionObj
{

    vector<BasicBlock*> conditionBBVec; 
    BasicBlock *branchTrueBeginBB,*branchTrueEndBB; 
    int flags;
    int IfObjNumber;    
}IfConditionObj;

typedef struct ArgStruct{
    Value *argval;  
    Type* argtype;  
    StringRef operandName;  
}ArgStruct;

typedef struct IfTemplate{
    Function *function; 
    ICmpInst *icmpInst; 
    vector<ArgStruct> Args;    

}IfTemplate;

void storeTheIfObj(vector<IfConditionObj> &AllIfObjInCurFunc,vector<BasicBlock*> resultVec,vector<BasicBlock*> rangeBB,int IfObjNumber);

bool checkIsIF(BasicBlock *basicblock);

void handleStateVarInIfScene(Module &M,map<Function*,int> CallGraphMap,vector<string> MeaningDict,vector<StateVar> MealyVarVec);



BasicBlock* getNotStartWithIfBasicBlock(BasicBlock *basicblock);

void traverseForIFCondition(BasicBlock *basicblock,vector<BasicBlock*> &resultVec);

void extractBranchTrue(BasicBlock *curBB,vector<BasicBlock*> &rangeBB);

bool checkIsTargetIFObj(IfConditionObj ifConditionObj,vector<IfTemplate> &temVec);


void analysisAllIfObjInCurFuncForMealyVar(vector<IfConditionObj> AllIfObjInCurFunc,vector<string> MeaningDict,int phase,vector<StateVar> MealyVarVec);

bool checkArgsIfConstr(Value *args);

Value* getArgsFromStrCmpFunction(CallInst *callInst);

bool checkIsTargetIFObjEqualTemplate(IfConditionObj ifConditionObj,vector<IfTemplate> temVec);


template<typename type1,typename type2>
void convertMaptoVec(map<type1,type2> countMap,vector<pair<type1,type2>> &vec);

int calIfComplexity(BasicBlock *beginBB,BasicBlock *endBB);


void handleCandidateIfObj(vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue);

bool iterateIFCodeForCallFeature(vector<IfConditionObj> AllIfObjInCurFunc,int flags,int length,vector<int> &candidateResVec);

bool checkFuncArgsIfRelateProtocol(CallInst *callInst,vector<vector<string>> MeaningDict);

void handleMultiSingleIFObj(vector<IfConditionObj> AllIfObjInCurFunc,vector<string> MeaningDict,int flags,int isMulti);

bool checkIFObjArgsRelateProtocol(vector<IfConditionObj> candidateIFObjVec,vector<vector<string>> MeaningDict);

void extractFuncArgsUsed(CallInst *callInst,map<StringRef,int> &CountArgsMap);

template<typename type1,typename type2>
bool cmp2(pair<type1,type2>&a,pair<type1,type2>&b);

BasicBlock* getTheTureBasicBlockofIF(BasicBlock *basicblock);

bool insertFunctionInOneCondition(IfConditionObj obj,vector<string> MeanDict,StringRef semantics,int isMulti);

void insertFunctionForIF(vector<IfConditionObj> AllIfObjInCurFunc,int flags,vector<string> MeanDict,StringRef semantics,int isMulti);

StringRef extractFunctionArgSemantics(Value *argval);

bool checkIfStrCmpFunction(CallInst *callinst,Function *calledFunction);

void extractIFObjInCurFunction(Function *function,vector<IfConditionObj> &AllIfObjInCurFunc);

void handleCandidateSwitchObj_NotStateCondition(vector<SwitchInstrumentObj> switchResults);

void extractSwCaseValue(SwitchInst *swInst,vector<int> &SwCaseValueVec);

int checkSingleIfCodeComplexity(IfConditionObj ifObjPtr,int complexity);

StringRef extractFirstIFVarSemantic(vector<IfConditionObj> AllIfObjInCurFunc,int flags);

Instruction* extractFirstIFVarInst(vector<IfConditionObj> AllIfObjInCurFunc,int flags,StringRef semantic);

bool checkIfSemanticInFuncArgs(CallInst *callInst,StringRef semantic);

void instrumentCandidateIfObj(vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue,vector<StateVar> MealyVarVec,vector<string> MeaningDict);

#endif
