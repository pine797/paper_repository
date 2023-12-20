#include "../include/cppheader.h"
#include "../include/llvmheader.h"

using namespace std;
using namespace llvm;


void extractCallGraph(Module &M,map<Function*,int> &CallGraphMap);

void extractAllRecvAndAccept(Module &M,vector<Function*> &readFuncVec,vector<Function*> &acceptFuncVec);

void backtraceForTargetFunc(Function *TargetFunc,vector<vector<Function*>> &AllParentFuncVec,int maxBackTraceLevel);


void backtraceOneLevel(vector<Function*> functionVec,vector<vector<Function*>> &AllParentFuncVec,queue<vector<Function*>> &fqueue,int ccurpos);

void matrixToMap(vector<vector<Function*>> AllParentRecvFuncVec,map<Function*,int> &resultMap);

void filterMergeNodeVec(Module &M,vector<Function*> &MergeNodeVec,vector<pair<Function*,int>> &MergeNodeVec_level);

void iterateCallSiteFromTargetFunc(Function *SourceFunc,map<Function*,int> &resultsMap,int maxLevel);

void iterateOneLevel(vector<Function*> beginVec,map<Function*,int> &resultsMap,int level,queue<vector<Function*>> &fqueue);

Function* chooseTheMostDeepFunction(Module &M,vector<Function*> MergeNodeVec,vector<Function*> recvParentFunc,vector<Function*> acceptParentFunc);

bool judge(const pair<Function*,int> a, const pair<Function* ,int> b);

bool judge_re(const pair<Function*,int> a, const pair<Function* ,int> b);


void unix_network_extractCallGraph(Module &M,map<Function*,int> &CallGraphMap_State,map<Function*,int> &CallGraphMap_All);

template <typename T> 
void uniqueVector(vector<T> &targetVec);

int findStateMachineCallGraphLevel(vector<pair<Function*,int>> resultVec,Function *callgraphBegin);

void extractGVFuncPointerInitialInfo(Module *M,GlobalVariable *FuncGV,vector<Function*> &functionVec);


int calLevelBetweenTargeFuncAndOthers(Module &M,Function *targetFunc,Function *otherFunc);

bool backtraceForCalLevel(vector<Function*> functionVec,queue<vector<Function*>> &fqueue,Function *targetFunc);


bool checkIfAwasCalledB(Function *A,Function *B);


typedef struct node 
{
    Function *function;
    node* next;
}EdgeNode;

typedef struct  
{
    Function *function;
    int level;
    int edgeCount;  
    EdgeNode* firstedge;
}VertexNode;

Function* createGraphToChoose(vector<pair<Function*,int>> resVec,vector<Function*> readVec,vector<Function*> acceptVec);


void checkGlobalVariablesIfCall(GlobalVariable *globalvar,vector<Function*> &resVec);

GlobalVariable* checkCalledObjIfPointToGlobalArray(Instruction *instruction);

void iterateGlobalArrayToStoreFunction(GlobalVariable *globalArray,vector<Function*> &storedVec);

bool checkIfBlackList(StringRef funcname);


bool checkFunctionIfContainCallInst(Function *function);