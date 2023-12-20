#include "../include/cppheader.h"
#include "../include/llvmheader.h"

using namespace std;
using namespace llvm;

/**
 * @description: 为CPP程序提取检测范围
 * @param {Module} &M
 * @param {map<Function*,int>} &CallGraphMap
 * @return {*}
 */
void extractCheckRangeForCPP(Module &M,map<Function*,int> &CallGraphMap);


/**
 * @description: 从TargetFunc开始向上回溯maxBackTraceLevel层,结果保存至AllParentFuncVec
 * @param {Function} *TargetFunc
 * @param {vector<vector<Function*>>} &AllParentFuncVec
 * @param {int} maxBackTraceLevel
 * @return {*}
 */
void backtraceForTargetFunc(Function *TargetFunc,vector<vector<Function*>> &AllParentFuncVec,int maxBackTraceLevel);

/**
 * @description: 以functionVec为起点向上回溯一层（backtraceForTargetFunc()辅助函数）
 * @param {vector<Function*> functionVec: 回溯的开端
 * @param vector<vector<Function*>> &AllParentFuncVec: 逐层保存回溯的函数
 * @param queue<vector<Function*>>} &fqueue
 * @param int backtraceOneLevel :值为0时候不保存，即不保存起始函数本身
 * @return {*}
 */
void backtraceOneLevel(vector<Function*> functionVec,vector<vector<Function*>> &AllParentFuncVec,queue<vector<Function*>> &fqueue,int ccurpos);
/**
 * @description: 将二维数组（Function*）转化为一维的map<Function*,int>
 * @param {vector<vector<Function*>> AllParentRecvFuncVec,map<Function*,int>} &resultMap
 * @return {*}
 */
void matrixToMap(vector<vector<Function*>> AllParentRecvFuncVec,map<Function*,int> &resultMap);
/**
 * @description: 过滤获取到的read()...和accept()回溯的交叉函数结点，如继续向上回溯5层，判断是否能够回溯至main函数
 * @param {Module} &M
 * @param {vector<Function*>} &MergeNodeVec 待检测的交叉函数点
 * @param {vector<pair<Function*,int>>} &MergeNodeVec_level 待检测的交叉函数及其与read函数的层数
 * @return {*}
 */
void filterMergeNodeVec(Module &M,vector<Function*> &MergeNodeVec,vector<pair<Function*,int>> &MergeNodeVec_level);
/**
 * @description: 从指定函数开始向下遍历提取Call function,保存至map<Function*,int>中
 * @param {Function} *SourceFunc 遍历起始函数
 * @param {map<Function*,int>} &resultsMap 保存结果
 * @param {int} maxLevel 指定向下遍历的层数
 * @return {*}
 */
void iterateCallSiteFromTargetFunc(Function *SourceFunc,map<Function*,int> &resultsMap,int maxLevel);
/**
 * @description: 由指定函数数组开始向下遍历指定层
 * @param {vector<Function*> tmpvec,
 * @param map<Function*,int> &resultsMap,
 * @param int level,
 * @param queue<vector<Function*>>} &fqueue
 * @return {*}
 */
void iterateOneLevel(vector<Function*> beginVec,map<Function*,int> &resultsMap,int level,queue<vector<Function*>> &fqueue);
/**
 * @description: 从Main向下遍历5层，选择最深的交叉函数点并返回
 * @param {Module} &M
 * @param {vector<Function*>} MergeNodeVec
 * @return {*}
 */
Function* chooseTheMostDeepFunction(Module &M,vector<Function*> MergeNodeVec,vector<Function*> recvParentFunc,vector<Function*> acceptParentFunc);

bool judge(const pair<Function*,int> a, const pair<Function* ,int> b);

bool judge_re(const pair<Function*,int> a, const pair<Function* ,int> b);

/**
 * @description: extract CallGraph
 * @param {Module} &M
 * @param {map<Function*,int>} &CallGraphMap
 * @return {*}
 */
void extractCallGraph(Module &M,map<Function*,int> &CallGraphMap);


/**
 * @description: vector去重（影响顺序）
 * @param {*}
 * @return {*}
 */
template <typename T> 
void uniqueVector(vector<T> &targetVec);
/**
 * @description: 选取状态机关键变量的callgraph匹配层数
 * @param {vector<Function,int>} resultVec
 * @return {*}
 */
int findStateMachineCallGraphLevel(vector<pair<Function*,int>> resultVec,Function *callgraphBegin);
/**
 * @description: 提取全局变量型函数指针的初始化情况
 * @param {GlobalVariable} *FuncGV 目标
 * @param {vector<Function*>} &functionVec 保存结果
 * @return {*}
 */
void extractGVFuncPointerInitialInfo(Module *M,GlobalVariable *FuncGV,vector<Function*> &functionVec);

/**
 * @description: 计算指定函数与Main函数之间的层数（确保targetFunc是回溯的开始，otherfunc是向上回溯的终点）
 * @param {Module} &M
 * @param {Function} *targetFunc:回溯的起始函数
 * @param {Function} *otherFunc:回溯终点函数
 * @return {*}
 */
int calLevelBetweenTargeFuncAndOthers(Module &M,Function *targetFunc,Function *otherFunc);

bool backtraceForCalLevel(vector<Function*> functionVec,queue<vector<Function*>> &fqueue,Function *targetFunc);

/**
 * @description: check if A was called by B
 * @param {Function} *A
 * @param {Function} *B
 * @return {*}
 */
bool checkIfAwasCalledB(Function *A,Function *B);

//=====邻接表定义=====
typedef struct node //边表节点
{
    Function *function;
    node* next;
}EdgeNode;

typedef struct  //顶点表节点
{
    Function *function;
    int level;//距离main的层数
    int edgeCount;  //边表节点的个数
    EdgeNode* firstedge;
}VertexNode;

Function* createGraphToChoose(vector<pair<Function*,int>> resVec,vector<Function*> readVec,vector<Function*> acceptVec);

/**
 * @description: 检查全局变量globalvar是否作为函数被调用
 * @param {GlobalVariable} *globalvar
 * @return {*}
 */
void checkGlobalVariablesIfCall(GlobalVariable *globalvar,vector<Function*> &resVec);

GlobalVariable* checkCalledObjIfPointToGlobalArray(Instruction *instruction);

void iterateGlobalArrayToStoreFunction(GlobalVariable *globalArray,vector<Function*> &storedVec);

bool checkIfTCPorUDP();

bool checkIfBlackList(StringRef funcname);

/**
 * @description: 检查函数中是否有call指令
 * @param {Function} *function
 * @return {*}
 */
bool checkFunctionIfContainCallInst(Function *function);