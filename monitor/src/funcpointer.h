#include "../include/cppheader.h"
#include "../include/llvmheader.h"

using namespace std;
using namespace llvm;

#define FuncPointer1    1
#define FuncPointer2    2
#define FuncPointer3    3

typedef struct CandidateObj{
    Function *function; //调用指令所在的函数
    Instruction *callInst;   //调用函数的那一条指令
    int situation;  //标识表示的场景

}CandidateObj;

void insertIntoCandiateObjForFuncPointer(Instruction *instruction,int situation);
/**
 * @description: std::sort()自定义排序方法，按<key,val>类型中val值大小进行递减排序
 * @param {*} 比较对象
 * @return {*} true / false
 */
bool cmp(pair<int,int>&a,pair<int,int>&b);

/**
 * @description: preprocess: iterate all globalvariables to extract the one stored function pointer from Module
 * @param {Module} &M : llvm module
 * @param {vector<StringRef>} &FuncPointerVec : stored globalvariables that stored function pointer here
 * @return {*}
 */
void extractGlobalVar(Module &M, vector<StringRef> &FuncPointerVec,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);

/**
 * @description: check if the target GlobalVariables stored function pointer
 * @param {GlobalVariable} &Global
 * @return {*} 
 */
bool checkGlobalArrayIfFunction(GlobalVariable &Global);

/**
 * @description: iterate all callsite inst to check if this callsite has message dispacher features
 * @param {Module} &M
 * @param {vector<StringRef> &FuncPointerVec:记录保存了函数指针的GlobalVariable名称 
 * @param {vector<StringRef>} &FuncPointerUsedVec: 记录使用了函数指针的
 * @param {vector<vector<string>>} MeaningDict: <函数语义字典, 参数语义字典>
 * @return {*}
 */
void checkAllCallSite(Module &M, vector<StringRef> &FuncPointerVec, vector<StringRef> &FuncPointerUsedVec, vector<vector<string>> MeaningDict,
    vector<StringRef> FuncUsedTargetStructVec,map<Function*,int> CallGraphMap_State);

/**
 * @description: check the function args to determine which args to monitor
 * @param {Module} &M
 * @param {Instruction} &instruction : callsite所属指令
 * @param {vector<vector<string>> MeaningDict : <函数语义字典, 参数语义字典>
 * @param vector<StringRef>} FuncPointerVec : 记录保存了函数指针的GlobalVariable名称
 * @return {*}
 */
bool checkFuncArgsMeaning(Module &M,Instruction &instruction,vector<vector<string>> MeaningDict,vector<StringRef> FuncPointerVec);
/**FIXME:较受限，考虑后期更改为层序遍历等方式...
 * @description: 提取函数参数的语义 
 * @param {Value} *arg
 * @return {*} 语义值
 */
StringRef getArgsMeaning(Value *arg,Instruction *instruction,int targetArgPos);

/**
 * @description: 检查val是否为i8*类型
 * @param {Value} *val
 * @return {*}
 */
bool checkIfI8Pointer(Value *val);

/**
 * @description: 1.iterate all function to check function pointer ,if a function contains function pointer ,then store this function name for later check use ; 2. iterate all function to check if it used the target struct type, then store it in FuncUsedTargetStructVec
 * @param {Module} &M
 * @param {vector<StringRef>} &FuncPointerUsedVec 保存使用了函数指针的数组
 * @param {vector<Type*>} &FunPointerArrayElementTypeValueVec 存储了目标结构体类型的数组，用于匹配
 * @param {vector<StringRef>} &FuncUsedTargetStructVec 用于存储包含目标结构体类型的函数名称
 * @return {*}
 */
void extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(Module &M,vector<StringRef> &FuncPointerUsedVec,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec);

/**
 * @description: check if the args is user defined structure type,now support two types : structure && structure * 
 * @param {Value} *val
 * @return {*}
 */
bool checkIfStructureType(Value *val);


/**
 * @description: iterate GlobalVariables that stored func pointer, then count the used times of func args which type is targetType in these function
 * @param {Module} &M
 * @param {vector<StringRef> FuncPointerVec
 * @param Type *targetType: 目标类型
 * @param unordered_map<int,int> &CounterMap: 统计结果
 * @param vector<StringRef>} &funcVec 记录检测了的函数
 * @return {*}
 */
void iterateGlobalFuncArrayByType(Module &M,vector<StringRef> FuncPointerVec,Type *targetType,unordered_map<int,int> &CounterMap,vector<StringRef> &funcVec);

/**
 * @description: count the used times of func args which type is targetType in these function
 * @param {Function} *function 检查函数
 * @param {Type} *targetType 目标类型
 * @param {unordered_map<int,int>} &CounterMap 统计结果
 * @return {*}
 */
void checkFuncArgUse(Function *function, Type *targetType,unordered_map<int,int> &CounterMap);

/**
 * @description: check if the type is i8*
 * @param {Type} *type
 * @return {*}
 */
bool checkTypeIfi8pointer(Type *type);

/**
 * @description: check if the type is i8**
 * @param {Type} *type
 * @return {*}
 */
bool checkTypeIfi8doublepointer(Type *type);

/**
 * @description: 检查当前指令是否是Gep指令，且操作类型是targetType，操作数1是FirstGepPos，操作数个数是GepOpearandNum
 * @param {Instruction} *instruction
 * @param {Type} *targetType
 * @param {int} FirstGepPos
 * @param {int} GepOpearandNum
 * @return {*}
 */
bool checkFirstGepInst(Instruction *instruction,Type *targetType,int FirstGepPos,int GepOpearandNum);

/**
 * @description: 针对structure**类型，遍历FuncPointerVec中的函数，统计指定类型的Gep指令的下一个（第二个）Gep指令的操作数value使用确定，返回最多使用的那个pos
 * @param {Module} &M
 * @param {vector<StringRef>} FuncPointerVec 待检测函数列表
 * @param {Type} *targetType First Gep指令的操作类型
 * @param {int} FirstGepPos First Gep指令的操作数
 * @return {*}
 */
int findSecondGepOperandVal(Module &M,vector<StringRef> FuncPointerVec,Type *targetType,int FirstGepPos);

/**
 * @description: 从当前Gep指令开始寻找下一个符合目标类型的Gep指令
 * @param {Instruction} *instruction
 * @param {unordered_map<int,int>} &CounterMap
 * @param {int} GepOperandNum
 * @return {*}
 */
void findTheNextGepFromCurInst(Instruction *instruction,unordered_map<int,int> &CounterMap,int GepOperandNum);
// check if the called function pointer stored in the target array
bool checkIfFuncPointerInArray(CallSite &callsite, vector<StringRef> &FuncPointerVec);

/**
 * @description: 遍历Module中的StructTypes,检查StructTypes中是否存在成员为FunPointerArrayElementTypeStringVec或FunPointerArrayElementTypeValueVec中的成员,有则更新两个数组
 * @param {Module} &M
 * @param {vector<string> &FunPointerArrayElementTypeStringVec: 保存具有目标类型的结构体字符串名称，用于检查Union时与dwarf信息交互
 * @param vector<Type*>} &FunPointerArrayElementTypeValueVec：保存具有目标类型的结构体IR地址，用于非Union类型的检查
 * @return {*}
 */
void extractGlobalStructContainsTargetType(Module &M,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);
/**
 * @description: 符合标准的目标类型存储至两数组中
 * @param {StringRef} structName
 * @param {Type} *structType
 * @param {vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*>} &FunPointerArrayElementTypeValueVec
 * @return {*}
 */
void storeStructTypeAndStringName(StringRef structName,Type *structType,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec);

/**
 * @description: 检查struct成员是否有目标类型
 * @param {Type} *type
 * @param {vector<string> FunPointerArrayElementTypeStringVec,
 * @param vector<Type*>} FunPointerArrayElementTypeValueVec
 * @return {*}
 */
bool checkStructMemberIfContainsTargetType(Type *type,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec);
/**
 * @description: 检查Union成员中是否包含目标类型
 * @param {Module} &M
 * @param {StringRef} structName
 * @param {int} unionPos
 * @param {vector<string> FunPointerArrayElementTypeStringVec,vector<Type*>} FunPointerArrayElementTypeValueVec
 * @return {*}
 */
bool checkUnionIfContainsTargetType(Module &M,StringRef structName,int unionPos,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec);
/**
 * @description: 递归检查Union的成员类型是否有目标Type的
 * @param {DIDerivedType} *curType
 * @param {vector<string>} FunPointerArrayElementTypeVec
 * @return {*}
 */
bool iterateUnionToCheckIfContainTargetStruct(DIDerivedType *curType,vector<string> FunPointerArrayElementTypeVec);
/**
 * @description: 检查type是否存在于FunPointerArrayElementTypeValueVec中，若存在，则将函数名称存入FuncUsedTargetStructVec
 * @param {Function} *function
 * @param {Type} *type
 * @param {vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef>} &FuncUsedTargetStructVec
 * @return {*}
 */
void checkTypeIfInArray(Function *function,Type *type,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec);

bool checkArgsMeaningIfOK_plus(vector<vector<string>> MeanDict,string args,int switchFlag,int &meaningPos);

void handleCandidateObjForFuncPointer(Module &M,vector<vector<string>> MeaningDict,vector<StringRef> FuncPointerVec);

Instruction* extractArrayIdx(BasicBlock *basicblock,vector<StringRef> FuncPointerVec);
