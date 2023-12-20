#include "../include/cppheader.h"
#include "../include/llvmheader.h"


#define statemachine_int_8_funcname "stateMachineVarMonitor_int8"
#define statemachine_int_32_funcname "stateMachineVarMonitor_int32"
#define statemachine_int_64_funcname "stateMachineVarMonitor_int64"
#define statemachine_char_funcname "stateMachineVarMonitor_str"

#define multistate_int_8_funcname "make_sensitive_for_multistate_int_8"
#define multistate_int_32_funcname "make_sensitive_for_multistate_int_32"
#define multistate_int_64_funcname "make_sensitive_for_multistate_int_64"
#define multistate_char_funcname "make_sensitive_for_multistate_char"

#define singlestate_int_8_funcname "make_sensitive_for_singlestate_int_8"
#define singlestate_int_32_funcname "make_sensitive_for_singlestate_int_32"
#define singlestate_int_64_funcname "make_sensitive_for_singlestate_int_64"
#define singlestate_char_funcname "make_sensitive_for_singlestate_char"

#define MultiScene  0
#define SingleScene 1
#define StateMachineScene 2

#define State_Char  0
#define State_Int   1

#define Var_Type_Char    0
#define Var_Type_Int8    1
#define Var_Type_Int16    2
#define Var_Type_Int32   3
#define Var_Type_Int64   4


#define Handle_Func_Pos 0
#define Call_Func_Pos 1
#define Protocol_Args_Pos    2
#define StateMachine_Args_Pos   3
#define New_StateMachine_Args_Pos   4

using namespace std;
using namespace llvm;

typedef struct {
    string filename; //原文件名称
    int codeline;   //代码行号
    string semantics;    //语义名称
    string funcname;    //函数名称
    int IsMultiOrSingle;    //0:Multi   1:Single
    int IsIntOrStr; //0:Int 1:Str
}InstrumentInfo;

//维护状态机变量识别所需要的一些列信息
typedef struct {
    vector<int> VarType;    //变量的类型，作为其他模块寻找statemachine变量的约束
    vector<int> SwCaseStateVal;//switch case的值，作为状态变量取值的候选
}StateMachineVarAssistInfo;


void extractInstInfoForMealyVar(Instruction *instruction,string varName,int type);

void InsertFunction(Module *M,Instruction *locateInst,Value *targetInst,int sceneType,int stateType,ArrayRef<Value*> funcArgs,int &error);

int identifyStateType(Instruction *instruction);

int identifyStateVarType(Instruction *instruction);

void addInstrumentInfoToVector(string filename,int codeline,string funcname,string semantics,int MultiOfSingle,int IntOrStr);

void showAllInstrumentLog();

bool checkArgsMeaningIfOK(vector<vector<string>> MeanDict,string args,int switchFlag,int strictMode);

int CountLines(StringRef filename);

string readContentFromTheFile(StringRef filename,int line);

bool StartWith(string s1,string s2);

//Word2Vec所需相关数据结构/变量
typedef struct {
    FILE *f;
    long long word;
    long long size;
    char *vocab;
    float *M;
}Word2VecData;

// word2vec model 初始化
void initialWord2VecModel(FILE *f,long long &words,long long &size,char *&vocab,float *&M,map<string,float*> &stateDictMap,vector<string> stateDicVec);
// 从model中提取字符串对应向量
bool extractStrVector(const char *str,long long words,char *vocab,long long size,float *M,float first[]);
// 检查两个向量之间的余弦相似度
bool checkIfSimilarity(float* first,float* second,long long size);
// 关闭word2vec
void closeWord2VecModel(Word2VecData *word2vecdata,map<string,float*> &stateDictMap);

/**
 * @description: 删除LLVM IR变量中非LLVM Info信息
 * @param {StringRef} varName
 * @return {*}
 */
StringRef delLLVMIRInfoForVar(StringRef varName);

void loadStateMachineCallGraph();

bool checkFuncInStateMachineCallGraph(StringRef filename);

string getVariableNameWithParent(Value *variable);