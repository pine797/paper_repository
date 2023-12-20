#include "../include/cppheader.h"
#include "../include/llvmheader.h"
#include "../include/switchmodule.h"


using namespace std;
using namespace llvm;



int findInstFromWhichIFObj(vector<IfConditionObj> AllIfObjInCurFunc,Instruction* instruction);


void countAllStructureUsing(Module &M);

void backtraceToGetStructureType(Instruction *instruction,map<Type*,int> &resultVec);

void backtraceToGetStructureType_onelevel(BasicBlock *basicblock,vector<Instruction*> &tmp,queue<vector<Instruction*>> &Iqueue,map<Type*,int> &resultVec);

bool checkInstructionIfIF(Instruction *instruction,vector<IfConditionObj> AllIfObjInCurFunc);

StringRef backtraceForMeaning(Instruction *instruction,BasicBlock *constrBasicblock);

void iterateAllSwAndIfCondition(Module &M,map<Function*,int> CallGraphMap,vector<string> MeaningDict);

StringRef processVariableNameWithoutNum(StringRef str);

bool checkIfValueFromArgs(Function *function,Instruction *instruction);

void handleCandidateResult(Function *function,map<string,vector<Instruction*>> candidateResultsMap,vector<IfConditionObj> AllIfObjInCurFunc,vector<string> MeaningDict);


bool checkInstructionIfIcmpCondition(BasicBlock *curBasicBlock,Instruction *instruction,Instruction *icmpInst);

bool checkBBSuccessorIfStartWithTargetStringref(BasicBlock *curbasicblock,StringRef targetString);

bool checkInstructionIfSwitch(Instruction *instruction,SwitchInst **res_switchInst);

bool checkArgsOnlyUseOnce(Function *function,Instruction *endInst,Argument *args,StoreInst **finalStoreInst);

bool checkIfInstFromArgs(Function *function,Instruction *instruction,int &ArgPos);

Instruction* determineWhitchToMonitor(Instruction *instruction);

bool checkIcmpInstFromTargetPoint(Instruction *targetPoint);