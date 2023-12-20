#include "./llvmheader.h"
#include "./cppheader.h"

using namespace std;
using namespace llvm;

typedef struct FunctionStackInfo{
    vector<string> funcStackVec;
    map<string,string> funcStackMap;
}FunctionStackInfo;

void test(Module *M);

void readRecvFuncStackInfo(string filepath,vector<FunctionStackInfo> &FuncStackVec);

Function* checkIfStackInfoContainsThread(Module &M,vector<FunctionStackInfo> &FuncStackVec);

Function* findStartFunction(Module *M,vector<FunctionStackInfo> &FuncStackVec,int &SkipFlags);

bool checkEntryFunction(Function *curFunction,vector<FunctionStackInfo> &FuncStackVec);

void extractCallGraph(Module &M,map<Function*,int> &CallGraphMap,vector<FunctionStackInfo> funcStackInfo,Function *EntryFunc,int CallLevel,int SkipBeforeCode);

void iterateCallSiteFromTargetFunc(Module &M,Function *SourceFunc,map<Function*,int> &resultsMap,vector<FunctionStackInfo> funcStackInfo,int maxLevel,int skipBeforeEvent);

void iterateOneLevel(Module &M,vector<Function*> beginVec,map<Function*,int> &resultsMap,vector<FunctionStackInfo> funcStackInfo,int level,queue<vector<Function*>> &fqueue, int skipBeforeEvent);

bool checkIfBlackList(Function *function);

bool checkFunctionIfContainCallInst(Function *function);

void extractGVFuncPointerInitialInfo(Module *M,GlobalVariable *FuncGV,vector<Function*> &functionVec);

GlobalVariable* checkCalledObjIfPointToGlobalArray(Instruction *instruction);

void iterateGlobalArrayToStoreFunction(GlobalVariable *globalArray,vector<Function*> &storedVec);

bool checkIfContaintsTwoList(string funcName,vector<string> WhiteList1,vector<string> WhiteList2);

void extractOtherCallGraph(Module &M,map<Function*,int> &CallGraphMap);

bool is_element_in_function_vector(vector<Function*> v,Function* element);