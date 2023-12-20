#include "../include/cppheader.h"
#include "../include/llvmheader.h"
#include "globalvar.h"


using namespace std;
using namespace llvm;

#define FuncPointer1    1
#define FuncPointer2    2
#define FuncPointer3    3

typedef struct CandidateObj{
    Function *function; 
    Instruction *callInst;   
    int situation;  
    int MealyVar;   
}CandidateObj;


void saveIdentifiedStateVarInDirectScene(Instruction *instruction,int situation,int MealyVar);

bool cmp(pair<int,int>&a,pair<int,int>&b);


void extractGlobalVar(Module &M, vector<StringRef> &FuncPointerVec,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);


bool checkGlobalArrayIfFunction(GlobalVariable &Global);


void handleIndirectScene3(Module &M,Instruction *instruction,vector<vector<string>> MeaningDict,vector<StringRef> FuncPointerVec);

StringRef getArgsMeaning(Value *arg,Instruction *instruction,int targetArgPos);


bool checkIfI8Pointer(Value *val);


void extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(Module &M,vector<StringRef> &FuncPointerUsedVec,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec);


bool checkIfStructureType(Value *val);



void iterateGlobalFuncArrayByType(Module &M,vector<StringRef> FuncPointerVec,Type *targetType,unordered_map<int,int> &CounterMap,vector<StringRef> &funcVec);


void checkFuncArgUse(Function *function, Type *targetType,unordered_map<int,int> &CounterMap);


bool checkTypeIfi8pointer(Type *type);


bool checkTypeIfi8doublepointer(Type *type);


bool checkFirstGepInst(Instruction *instruction,Type *targetType,int FirstGepPos,int GepOpearandNum);


int findSecondGepOperandVal(Module &M,vector<StringRef> FuncPointerVec,Type *targetType,int FirstGepPos);


void findTheNextGepFromCurInst(Instruction *instruction,unordered_map<int,int> &CounterMap,int GepOperandNum);

bool checkIfFuncPointerInArray(CallSite &callsite, vector<StringRef> &FuncPointerVec);


void extractGlobalStructContainsTargetType(Module &M,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);

void storeStructTypeAndStringName(StringRef structName,Type *structType,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);


bool checkStructMemberIfContainsTargetType(Type *type,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec);

bool checkUnionIfContainsTargetType(Module &M,StringRef structName,int unionPos,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec);

bool iterateUnionToCheckIfContainTargetStruct(DIDerivedType *curType,vector<string> FunPointerArrayElementTypeVec);

void checkTypeIfInArray(Function *function,Type *type,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec);

bool checkArgsMeaningIfOK_plus(vector<vector<string>> MeanDict,string args,int switchFlag,int &meaningPos);

void handleCandidateObjForFuncPointer(Module &M,vector<string> StateVarDict,vector<StringRef> FuncPointerVec);


Instruction* extractArrayIdx(BasicBlock *basicblock,vector<StringRef> FuncPointerVec);

void extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(Module &M,vector<StringRef> &FuncPointerUsedVec,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec,map<Function*,int> CallGraphMap);


void checkAllCallSite_Candidate_Mealy(Module &M, vector<StringRef> &FuncPointerVec, vector<StringRef> &FuncPointerUsedVec, vector<StringRef> FuncUsedTargetStructVec,map<Function*,int> CallGraphMap,vector<string> stateMachineCallGraph);


void saveStateVarInDirectScene(Module &M, vector<StringRef> &FuncPointerVec, vector<StringRef> &FuncPointerUsedVec, vector<StringRef> FuncUsedTargetStructVec,vector<StateVar> MealyVarVec,vector<StateVar> SubVarVec,map<Function*,int> CallGraphMap,vector<string> stateMachineCallGraph);
