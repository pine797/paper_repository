#include "../include/llvmheader.h"
#include "../include/cppheader.h"
#include "funcpointer.h"
#include "globalvar.h"

using namespace std;
using namespace llvm;

extern int curStateMachineNum;
extern int curSubStateNum;

extern vector<Value*> AlreadyMonitorVarVec;

vector<CandidateObj*> CandidateObjForFuncPointer;

void insertIntoCandidateObjForFuncPointer(Instruction *instruction,int situation){
    CandidateObj *obj = new CandidateObj;
    obj->function = instruction->getFunction();
    obj->callInst = instruction;
    obj->situation = situation;

    CandidateObjForFuncPointer.push_back(obj);
}

extern Word2VecData word2vecdata;
extern bool IfEnableWord2vec;

// /**
//  * @description: std::sort()自定义排序方法，按<key,val>类型中val值大小进行递减排序
//  * @param {*} 单一比较对象
//  * @return {*} true / false
//  */
// bool cmp(pair<int,int>&a, pair<int,int>&b) {
// 	return a.second > b.second;
// }


/**
 * @description: check if the target GlobalVariables stored function pointer
 * @param {GlobalVariable} &Global
 * @return {*} 
 */
void extractGlobalVar(Module &M, vector<StringRef> &FuncPointerVec,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec)
{
    for (auto &Global : M.getGlobalList())
    {
        if (Global.hasInitializer() && Global.getInitializer()->getNumOperands() > 0)
        {
            if (checkGlobalArrayIfFunction(Global))
            {
                // if the globalvariables stored function pointer, then store the globalvariabels addr
                // dbgs() << "GlobalVariable " << Global.getName() << " <stored> function pointer\n";
                FuncPointerVec.push_back(Global.getName());

                if (Global.getType()->isPointerTy() && Global.getType()->getElementType()->isArrayTy() && Global.getType()->getElementType()->getArrayElementType()->isStructTy())
                {
                    StringRef structName = Global.getType()->getElementType()->getArrayElementType()->getStructName();
                    storeStructTypeAndStringName(structName,Global.getType()->getElementType()->getArrayElementType(),FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
                }
            }
        }
    }
}
/**
 * @description: 1.iterate all function to check function pointer ,if a function contains function pointer ,then store this function name for later check use ; 2. iterate all function to check if it used the target struct type, then store it in FuncUsedTargetStructVec
 * @param {Module} &M
 * @param {vector<StringRef>} &FuncPointerUsedVec 保存使用了函数指针的数组
 * @param {vector<Type*>} &FunPointerArrayElementTypeValueVec 存储了目标结构体类型的数组，用于匹配
 * @param {vector<StringRef>} &FuncUsedTargetStructVec 用于存储包含目标结构体类型的函数名称
 * @return {*}
 */
void extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(Module &M,vector<StringRef> &FuncPointerUsedVec,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec)
{
    for (auto &function : M.getFunctionList())
    {    
        if (function.isDeclaration())
            continue;
        // if (!function.getName().equals(StringRef("pr_stash_get_symbol2")))
        //     continue;
        //TODO:限制检测的范围，来源CallGraph
        for (auto &basicblock : function)
        {
            for (Instruction &instruction : basicblock)
            {
                CallSite callsite(&instruction);
                if (callsite)
                {
                    if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction.getOperand(0)))
                    {
                        if (cstexpr->isCast())
                        {
                            Instruction *callInstruction = dyn_cast<Instruction>(cstexpr->stripPointerCasts());
                            if (callInstruction)
                            {
                                // errs()<<"store "<<instruction<<"\twhich in "<<instruction.getFunction()->getName()<<"\n";
                                FuncPointerUsedVec.push_back(instruction.getFunction()->getName());
                            }
                        }
                    }
                    else
                    {
                        if (isa<Instruction>(callsite.getCalledValue()))
                        {
                            // errs()<<"store "<<instruction<<"\twhich in "<<instruction.getFunction()->getName()<<"\n";
                            FuncPointerUsedVec.push_back(instruction.getFunction()->getName());
                        }
                    }
                }else{
                    //FIXME:考虑Store、Load的话范围较大，暂且仅考虑GetElementInst
                    // errs()<<instruction<<"\n";
                    // errs()<<"FunPointerArrayElementTypeValueVec size : "<<FunPointerArrayElementTypeValueVec.size()<<"\n";
                    //新增的：检查函数内部是否使用了目标类型
                    // if (LoadInst *loadinst = dyn_cast<LoadInst>(&instruction))
                    // {
                    //     // errs()<<"LoadInst:\t"<<*loadinst->getType()<<"\n";
                    //     if (loadinst->getType()->isPointerTy())
                    //     {
                    //         PointerType *pointer = cast<PointerType>(loadinst->getType());
                    //         // errs()<<"LoadInst:\t"<<*pointer->getElementType()<<"\n";
                    //         checkTypeIfInArray(&function,pointer->getElementType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                    //     }else if (loadinst->getType()->isStructTy())
                    //     {
                    //         checkTypeIfInArray(&function,loadinst->getType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                    //     }
                    // }else if (StoreInst *storeinst = dyn_cast<StoreInst>(&instruction))
                    // {
                    //     // errs()<<"StoreInst:\t"<<*storeinst->getOperand(0)->getType()<<"\n";
                    //     if (storeinst->getOperand(0)->getType()->isPointerTy())
                    //     {
                    //         PointerType *pointer = cast<PointerType>(storeinst->getOperand(0)->getType());
                    //         // errs()<<"StoreInst:\t"<<*pointer->getElementType()<<"\n";
                    //         checkTypeIfInArray(&function,pointer->getElementType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                    //     }else if (storeinst->getType()->isStructTy())
                    //     {
                    //         checkTypeIfInArray(&function,storeinst->getType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                    //     }   
                    // }else if (GetElementPtrInst *GePInst = dyn_cast<GetElementPtrInst>(&instruction))
                    if (GetElementPtrInst *GePInst = dyn_cast<GetElementPtrInst>(&instruction))
                    {
                        // errs()<<"GePInst:\t"<<*GePInst<<"\t"<<*GePInst->getOperand(0)->getType()<<"\n";
                        if (GePInst->getOperand(0)->getType()->isPointerTy())
                        {
                            PointerType *pointer = cast<PointerType>(GePInst->getOperand(0)->getType());
                            // errs()<<"GePInst:\t"<<*GePInst<<"\t"<<*pointer->getElementType()<<"\n";
                            checkTypeIfInArray(&function,pointer->getElementType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                        }
                        // else if (GePInst->getType()->isStructTy())
                        // {
                        //     checkTypeIfInArray(&function,GePInst->getType(),FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);
                        // }   
                    }
                }
            }
        }
    }
    //再向下处理一层
    for (auto &function : M.getFunctionList())
    {
        if (function.isDeclaration())
            continue;
        for (auto &basicblock : function)
        {
            for (auto &instruction : basicblock)
            {
                CallSite callsite(&instruction);
                if (callsite)
                {
                    if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction.getOperand(0)))
                    {
                        if (cstexpr->isCast())
                        {
                            Instruction *callInstruction = dyn_cast<Instruction>(cstexpr->stripPointerCasts());
                            if (callInstruction)
                            {
                                if (find(FuncUsedTargetStructVec.begin(),FuncUsedTargetStructVec.end(),instruction.getFunction()->getName())!=FuncUsedTargetStructVec.end())
                                {
                                    FuncUsedTargetStructVec.push_back(function.getName());
                                }
                            }
                        }
                    }
                    else
                    {
                        if (isa<Instruction>(callsite.getCalledValue()))
                        {
                            if (find(FuncUsedTargetStructVec.begin(),FuncUsedTargetStructVec.end(),instruction.getFunction()->getName())!=FuncUsedTargetStructVec.end())
                            {
                                FuncUsedTargetStructVec.push_back(function.getName());
                            }
                        }
                    }
                }
            }
        }        
    }
    //unique
    set<StringRef> ss(FuncUsedTargetStructVec.begin(),FuncUsedTargetStructVec.end());
    FuncUsedTargetStructVec.assign(ss.begin(),ss.end());    

    // errs()<<"extractFuncPointerUsedFuncname() -> FuncPointerUsedVec size: "<<FuncPointerUsedVec.size()<<"\n";
    // errs()<<"extractFunctionContainTargetStructType() -> FuncUsedTargetStructVec size: "<<FuncUsedTargetStructVec.size()<<"\n";
}

// check if the Global stored function pointer
bool checkGlobalArrayIfFunction(GlobalVariable &Global)
{
    int funcPointerNum = 0, arrayNum = Global.getInitializer()->getNumOperands() + 1;

    // iterate each element of the array(element is a function pointer or structure pointer whitch contains function pointer)
    Use *operand = Global.getInitializer()->getOperandList();
    for (int i = 0; i < Global.getInitializer()->getNumOperands(); i++)
    {
        Function *func = dyn_cast<Function>(operand[i].get());
        if (func != NULL && !func->isDeclaration())
        {
            // if element is user defined function
            funcPointerNum++;
        }
        else
        {
            // if element is structure rather than user defined function, then iterate the member value of the structure, see whether exist function pointer
            Constant *gv = dyn_cast<Constant>(operand[i].get());
            if (gv != NULL && gv->getType()->isStructTy())
            {
                // If structure ——> so iterate the member value
                for (Use &U : gv->operands())
                {
                    Function *call = dyn_cast<Function>(U.get());
                    // if one member is auser defined function ——> count number
                    if (call != NULL && !call->isDeclaration())
                    {
                        funcPointerNum++;
                    }
                }
            }
        }
    }
    if (funcPointerNum >= arrayNum * 0.8)
    {
        return true;
    }
    return false;
}

/**
 * @description: iterate all callsite inst to check if this callsite has message dispacher features
 * @param {Module} &M
 * @param {vector<StringRef> &FuncPointerVec:记录保存了函数指针的GlobalVariable名称 
 * @param {vector<StringRef>} &FuncPointerUsedVec: 记录使用了函数指针的
 * @param {vector<vector<string>>} MeaningDict: <函数语义字典, 参数语义字典>
 * @param {map<Function*,int>} CallGraphMap_State : 限制检测的Function范围
 * @return {*}
 */
void checkAllCallSite(Module &M, vector<StringRef> &FuncPointerVec, vector<StringRef> &FuncPointerUsedVec, vector<vector<string>> MeaningDict,
    vector<StringRef> FuncUsedTargetStructVec,map<Function*,int> CallGraphMap_State)
{
    bool FirstLoopRes = false;
    vector<StringRef> funcPointerStoredName;
    for (auto &function : M.getFunctionList())
    {
        //限制检测的Function数量，范围来源：CallGraph、NLP识别的分派函数名称
        if (CallGraphMap_State.find(&function) == CallGraphMap_State.end())
            continue;
        //NLP:检查
        // if (!checkArgsMeaningIfOK(MeaningDict,function.getName().lower(),Handle_Func_Pos))
        // {
        //     continue;
        // }
        // if (function.getName().str() != "_dispatch")
        //     continue;
        // errs()<<"函数名称: "<<function.getName()<<"\n";
        for (auto &basicblock : function)
        {
            for (Instruction &instruction : basicblock)
            {
                CallSite callsite(&instruction);
                if (callsite)
                {
                    // errs()<<"Instruction: "<<instruction<<"\n";
                    // [situation1 : called function has casted] , e.g. call void (...) bitcast (void ()* @expand_groups to void (...)*)()
                    if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction.getOperand(0)))
                    {
                        if (cstexpr->isCast())
                        {
                            // 暂不支持这种情况
                            // Function *func = dyn_cast<Function>(cstexpr->stripPointerCasts());
                            // if (func && FuncPointerVec.size() > 0)
                            // {
                                
                            // }
                        }
                    }
                    else
                    {
                        // [situation2 : call function pointer] , e.g. call void %12(i8* %13)
                        if (isa<Instruction>(callsite.getCalledValue()) && FuncPointerVec.size() > 0) //FuncPointerVec.size()>0 即默许函数指针的源头是一个数组，必须有这种数组，才检测这种情况
                        {
                            // errs() << "函数指针调用: " << instruction << "\n";
                            /*
                                1.检查call的函数主体%function，是否指向一个数组，且是我们识别出来的数组 √
                                2.检查函数参数，以确定要监控哪个参数，监控它    × ——> 应监控数组的下标
                                3.返回true，置flags（）

                            */
                            if (checkIfFuncPointerInArray(callsite, FuncPointerVec))
                            {
                                // 保存候选对象，待后续处理
                                insertIntoCandidateObjForFuncPointer(&instruction,FuncPointer2);

                                // FIXME:更改为监控全局数组的索引

                                // // errs()<<"参数信息如下:\n";
                                // int results = checkFuncArgsMeaning(M,instruction,MeaningDict,FuncPointerVec);
                                // errs()<<"results == "<<results<<"\n";
                                // if (results)
                                // {
                                //     //TODO:插桩OK,置flags，打印日志等信息
                                //     FirstLoopRes = true;
                                // }
                            }
                        }
                    }
                }
            }
        }
    }
    if (FirstLoopRes)
    {
        goto END_HANDLE;
    }

    // for situation3: [situation3 : call function], e.g. call void (i8, i8*, ...) @control_printf(...)
    for (auto &function : M.getFunctionList())
    {
        //限制检测的Function数量，范围来源：CallGraph、NLP识别的分派函数名称
        if (CallGraphMap_State.find(&function) == CallGraphMap_State.end())
            continue;
        //NLP语义
        if (!checkArgsMeaningIfOK(MeaningDict,function.getName().lower(),Handle_Func_Pos,0))
        {
            continue;
        }
        // if (function.getName().str() != "_dispatch")
        //     continue;
        // errs()<<"函数名称: "<<function.getName()<<"\n";
        for (auto &basicblock : function)
        {
            for (Instruction &instruction : basicblock)
            {
                CallSite callsite(&instruction);
                if (callsite)
                {
                    // errs()<<"Instruction: "<<instruction<<"\n";
                    // [situation1 : called function has casted] , e.g. call void (...) bitcast (void ()* @expand_groups to void (...)*)()
                    if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction.getOperand(0)))
                    {
                        //FIXME:暂不支持这种情况
                        continue;
                    }
                    else if (isa<Function>(callsite.getCalledValue()) && callsite.getCalledFunction()->isDeclaration()==0)
                    {
                        vector<StringRef>::iterator results = find(FuncPointerUsedVec.begin(),FuncPointerUsedVec.end(),callsite.getCalledFunction()->getName());
                        if (results!=FuncPointerUsedVec.end())
                        {
                            //if the function doesn't have the target struct type and so on , then skip the next step
                            vector<StringRef>::iterator res2 = find(FuncUsedTargetStructVec.begin(),FuncUsedTargetStructVec.end(),function.getName());
                            if (res2 == FuncUsedTargetStructVec.end())
                            {
                                continue;
                            }
                            // 保存候选对象，待后续处理
                            insertIntoCandidateObjForFuncPointer(&instruction,FuncPointer3);
                            // errs() << "\n候选对象，检查语义：显示函数调用: " << instruction << "\t" << callsite.getCalledFunction()->isDeclaration() << "\t最终的函数: "<<function.getName()<<"\n";
                            // int results = checkFuncArgsMeaning(M,instruction,MeaningDict,FuncPointerVec);
                            // if (results)
                            // {
                            //     //OK
                            // }
                        }
                    }
                }
            }
        }
    }
END_HANDLE:
    return;
}
// check if the called function pointer stored in the target array
bool checkIfFuncPointerInArray(CallSite &callsite, vector<StringRef> &FuncPointerVec)
{
    // we think the situation that call function pointer , the first instruction must be LoadInst
    if (LoadInst *loadinst = dyn_cast<LoadInst>(callsite.getCalledValue()))
    {
        // errs() << *loadinst << "\t" << *loadinst->getOperand(0) << "\n";
        if (GetElementPtrInst *gep1 = dyn_cast<GetElementPtrInst>(loadinst->getOperand(0)))
        {
            // if the first gepinst's operand(0) is already Global Array, then check it
            if (Constant *FinalArray1 = dyn_cast<Constant>(gep1->getOperand(0))){
                // errs() << "FinalArray1->getName(): " <<FinalArray1->getName() << "\n";
                vector<StringRef>::iterator result = find(FuncPointerVec.begin(),FuncPointerVec.end(),FinalArray1->getName());
                if (result != FuncPointerVec.end())
                {
                    errs()<<"callsite: "<<*callsite.getInstruction()<<"\t在"<<result->str()<<"\n中";
                    return true;
                }  
            }
            // if the first gepinst's operand(0) is still gepinst
            if (GetElementPtrInst *gep2 = dyn_cast<GetElementPtrInst>(gep1->getOperand(0)))
            {
                // if the second gepinst's operand(0) is Global Array, then check it
                if (Constant *FinalArray2 = dyn_cast<Constant>(gep2->getOperand(0))){
                    // errs() << "FinalArray2->getName(): " <<FinalArray2->getName() << "\n";
                    vector<StringRef>::iterator result = find(FuncPointerVec.begin(),FuncPointerVec.end(),FinalArray2->getName());
                    if (result != FuncPointerVec.end())
                    {
                        errs()<<"callsite: "<<*callsite.getInstruction()<<"\t在"<<result->str()<<"\n中";
                        return true;
                    }  
                }
            }
        }
    }
    return false;
}
/**
 * @description: check the function args to determine which args to monitor
 * @param {Module} &M
 * @param {Instruction} &instruction : callsite所属指令
 * @param {vector<vector<string>> MeaningDict : <函数语义字典, 参数语义字典>
 * @param vector<StringRef>} FuncPointerVec : 记录保存了函数指针的GlobalVariable名称
 * @return {*}
 */
bool checkFuncArgsMeaning(Module &M,Instruction &instruction,vector<vector<string>> MeaningDict,vector<StringRef> FuncPointerVec){
    vector<int> i8PointerPos;
    vector<int> StructPos;

    Use *args = instruction.getOperandList();
    if (instruction.getNumOperands()==1)
    {
        errs()<<"参数都没有监控什么\n";
        return false;
    }

    for (int i = 0; i < instruction.getNumOperands() - 1; i++)
    {
        // errs()<<"i:"<<i<<"\t"<<getArgsMeaning(args[i].get())<<"\t是否是i8*: "<<checkIfI8Pointer(args[i].get())<<"\n";
        Instruction *ArgsInst = dyn_cast<Instruction>(args[i].get());
        if (!ArgsInst)
        {
            continue;
        }
        // if the type of arg[i] is i8*
        if (checkIfI8Pointer(args[i].get()))
        {
            i8PointerPos.push_back(i);
        }
        else if (checkIfStructureType(args[i].get()))
        {
            // if the type of arg[i] is structure or structure *
            StructPos.push_back(i);
        }
    }
    if (i8PointerPos.size() == 1)
    {
        //检查函数参数的语义值，是否与消息请求类型相关
        Value *funcArg = args[i8PointerPos[0]].get();
        Instruction *ArgsInst = cast<Instruction>(funcArg);

        StringRef argval = getArgsMeaning(funcArg,&instruction,0);
        if (argval.empty())
        {
            return false;
        }else if (checkArgsMeaningIfOK(MeaningDict,argval.lower(),Call_Func_Pos,0)){
            //插桩监控
            errs()<<"[插桩监控]只有一个i8*,直接监控它: "<<*funcArg<<"\t语义是:"<<argval<<"\t类型是:"<<*funcArg->getType()<<"\n";
            IRBuilder <>IRB(ArgsInst->getParent());
            
            // 构造函数参数
            // StringRef filename = instruction.getFunction()->getSubprogram()->getFilename();
            // int codeline = instruction.getFunction()->getSubprogram()->getLine();
            // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(filename);
            // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),codeline,true);
            // Constant *Constant_curMultiStateNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curMultiStateNum,true);
            Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curStateMachineNum,true);
            int StateType = identifyStateType(ArgsInst);
            if (StateType == -1){
                return false;
            }
            ArrayRef<Value*> funcArgsArray = {ArgsInst,Constant_curStateMachineNum};
            // 插入具体函数
            int error = 0;
            InsertFunction(&M,ArgsInst,ArgsInst,StateMachineScene,StateType,funcArgsArray,error);
            if (error==0)
            {
                AlreadyMonitorVarVec.push_back(ArgsInst);
                string wholepath = instruction.getFunction()->getSubprogram()->getDirectory().str() + "/" + instruction.getFunction()->getSubprogram()->getFilename().str();
                addInstrumentInfoToVector(wholepath,ArgsInst->getDebugLoc()->getLine(),ArgsInst->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(ArgsInst->getName()),StateMachineScene,StateType);
                return true;
            }
        }
        return false;
    }else if (i8PointerPos.size() > 1){
        for (auto pos : i8PointerPos)
        {
            errs()<<"有多个i8*,结合语义进一步的筛选\n";
            Value *funcArg = args[pos].get();
            Instruction *ArgsInst = cast<Instruction>(funcArg);

            StringRef argval = getArgsMeaning(funcArg,&instruction,0);
            if (argval.empty())
            {
                return false;
            }else if (checkArgsMeaningIfOK(MeaningDict,argval.lower(),Call_Func_Pos,0)){
                //插桩监控
                errs()<<"[插桩监控]符合语义,直接监控它: "<<*funcArg<<"\t语义是:"<<argval<<"\n";
                IRBuilder <>IRB(ArgsInst->getParent());
                // 构造函数参数
                // StringRef filename = instruction.getFunction()->getSubprogram()->getFilename();
                // int codeline = instruction.getFunction()->getSubprogram()->getLine();
                // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(filename);
                // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),codeline,true);
                // Constant *Constant_curMultiStateNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curMultiStateNum,true);
                Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curStateMachineNum,true);
                int StateType = identifyStateType(ArgsInst);
                if (StateType == -1){
                    continue;
                }
                ArrayRef<Value*> funcArgsArray = {ArgsInst,Constant_curStateMachineNum};
                // 插入具体函数
                int error = 0;
                // InsertFunction(&M,ArgsInst,ArgsInst,MultiScene,StateType,funcArgsArray,error);
                InsertFunction(&M,ArgsInst,ArgsInst,StateMachineScene,StateType,funcArgsArray,error);
                if (error==0)
                {
                    AlreadyMonitorVarVec.push_back(ArgsInst);
                    string wholepath = ArgsInst->getFunction()->getSubprogram()->getDirectory().str() + "/" + ArgsInst->getFunction()->getSubprogram()->getFilename().str();
                    addInstrumentInfoToVector(wholepath,ArgsInst->getDebugLoc()->getLine(),ArgsInst->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(ArgsInst->getName()),StateMachineScene,StateType);
                    return true;
                }
            }
        }
        return false;
    }else{
        errs()<<"没有i8*,检查结构体的成员，以决定监控谁\n";
        unordered_map<int,int> CounterMap;
        vector<pair<int,int>> CounterMapVec;
        vector<StringRef> funcVec;

        for (int i = 0; i < StructPos.size(); i++)
        {
            StringRef argval = getArgsMeaning(args[StructPos[i]].get(),&instruction,i);
            if (argval.empty())
            {
                continue;
            }
            
            if (checkArgsMeaningIfOK(MeaningDict,argval.lower(),Call_Func_Pos,0)){
                // errs()<<"i: "<<i<<"语义匹配成功\t"<<*args[StructPos[i]].get()<<"\t语义:"<<getArgsMeaning(args[StructPos[i]].get())<<"\t类型:"<<*args[StructPos[i]].get()->getType()<<"\n";
                /*
                    问题：有候选结构体，但是不知道监控结构体的哪个成员
                    方案：用候选结构体和预处理时得到的包含函数指针的Global Array进行分析，例如检查这些Array中的函数指针的参数类型是否和候选结构体一样；
                        如果一样，则遍历FUnction中的GEP指令，统计对结构体的各成员访问次数，最终选取最多的那个并且是i8*类型的，作为监控的变量。
                    理论依据：函数指针调用，一定有数组来存要调用的函数，并且由于参数是待定的且是结构体，所以调用函数内部一定有GEP指令去访问其某个成员。
                */
                iterateGlobalFuncArrayByType(M,FuncPointerVec,args[StructPos[i]].get()->getType(),CounterMap,funcVec);
                // errs()<<"CounterMap size: "<<CounterMap.size()<<"\n";
                for (auto it : CounterMap)
                {
                    CounterMapVec.push_back(it);
                }
                std::sort(CounterMapVec.begin(),CounterMapVec.end(),cmp);
                if (CounterMapVec.size()==0)
                {
                    continue;
                }
                
                int firstPos = CounterMapVec[0].first;//获取使用次数最多的结构体成员下标
                
                PointerType *ptrtype = dyn_cast<PointerType>(args[StructPos[i]].get()->getType());

                if (StructType *stype = dyn_cast<StructType>(ptrtype->getElementType()))
                {
                    // 获取到结构体成员的type
                    Type *memberType = stype->getElementType(firstPos);
                    if (checkTypeIfi8pointer(memberType))
                    {
                        errs()<<"结构体成员是i8*，直接监控就完事了，但需要构造一个GEP和Load指令\n";
                        Value *argVal = args[StructPos[i]].get();


                        Value *indexList[2]= {ConstantInt::get(Type::getInt32Ty(instruction.getContext()),0),ConstantInt::get(Type::getInt32Ty(instruction.getContext()),firstPos)};
                        // string gepname = "multi_gep" + to_string(curMultiStateNum);
                        string gepname = "multi_gep" + to_string(curSubStateNum);

                        IRBuilder <>IRB(instruction.getParent());
                        IRB.SetInsertPoint(&instruction);
                        IRB.CreateInBoundsGEP(stype,argVal,ArrayRef<Value*>(indexList,2),StringRef(gepname.c_str()));

                        LoadInst *loadinst = IRB.CreateLoad(instruction.getPrevNode());
                        loadinst->setMetadata(instruction.getFunction()->getParent()->getMDKindID("nosanitize"),MDNode::get(instruction.getFunction()->getContext(),None));
                        
                        //构造函数参数
                        // StringRef filename = instruction.getFunction()->getSubprogram()->getFilename();
                        // int codeline = instruction.getFunction()->getSubprogram()->getLine();
                        // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(filename);
                        // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),codeline,true);
                        // Constant *Constant_curMultiStateNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curMultiStateNum,true);
                        Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curStateMachineNum,true);

                        int StateType = identifyStateType(loadinst);
                        if (StateType == -1){
                            continue;
                        }
                        ArrayRef<Value*> funcArgsArray = {loadinst,Constant_curStateMachineNum};
                        // 插入具体函数
                        int error = 0;
                        InsertFunction(&M,loadinst,loadinst,StateMachineScene,StateType,funcArgsArray,error);
                        AlreadyMonitorVarVec.push_back(loadinst);
                        if (error==0){
                            //导出插桩日志
                            string wholepath = instruction.getFunction()->getSubprogram()->getDirectory().str() + "/" + instruction.getFunction()->getSubprogram()->getFilename().str();
                            addInstrumentInfoToVector(wholepath,instruction.getDebugLoc()->getLine(),instruction.getFunction()->getName().str(),delLLVMIRInfoForVar(instruction.getOperand(0)->getName()),StateMachineScene,StateType);

                            return true;
                        }

                    }else if (checkTypeIfi8doublepointer(memberType))
                    {
                        errs()<<"结构体成员是i8**\n";
                        //由findSecondGepOperandVal()返回值提供数据
                        int secondPos = findSecondGepOperandVal(M,funcVec,args[StructPos[i]].get()->getType(),firstPos);
                        if (secondPos!=-1)
                        {
                            Value *argVal = args[StructPos[i]].get();
                            // Instruction *inst = cast<Instruction>(argVal);
                            errs()<<"获取下标找到啦:"<<firstPos<<"——"<<secondPos<<"\n";
                            // errs()<<"原始load "<<*args[StructPos[i]].get()<<"\n";
                            // errs()<<"GEP的操作数"<<*stype<<"\t"<<*ConstantInt::get(Type::getInt32Ty(inst->getParent()->getContext()),0)<<"\t"<<*ConstantInt::get(Type::getInt32Ty(inst->getParent()->getContext()),firstPos)<<"\n";
                            // //构造GEP->LOAD->GEP->LOAD指令插桩监控
                            Value *indexList1[2]= {ConstantInt::get(Type::getInt32Ty(instruction.getContext()),0),ConstantInt::get(Type::getInt32Ty(instruction.getContext()),firstPos)};
                            Value *indexList2[1]= {ConstantInt::get(Type::getInt32Ty(instruction.getContext()),secondPos)};
                            // // GetElementPtrInst *gepInst = GetElementPtrInst::CreateInBounds();
                            // string gepname = "multi_gep" + to_string(curMultiStateNum);
                            string gepname = "multi_gep" + to_string(curSubStateNum);

                            IRBuilder <>IRB(instruction.getParent());
                            IRB.SetInsertPoint(&instruction);
                            IRB.CreateInBoundsGEP(stype,argVal,ArrayRef<Value*>(indexList1,2),StringRef(gepname.c_str()));

                            // // IRB.SetInsertPoint(&instruction);
                            LoadInst *loadinst1 = IRB.CreateLoad(instruction.getPrevNode());
                            loadinst1->setMetadata(instruction.getFunction()->getParent()->getMDKindID("nosanitize"),MDNode::get(instruction.getFunction()->getContext(),None));
                            IRB.CreateInBoundsGEP(Type::getInt8PtrTy(instruction.getContext()),instruction.getPrevNode(),ArrayRef<Value*>(indexList2,1));
                            
                            LoadInst *loadinst2 = IRB.CreateLoad(instruction.getPrevNode());
                            loadinst2->setMetadata(instruction.getFunction()->getParent()->getMDKindID("nosanitize"),MDNode::get(instruction.getFunction()->getContext(),None));

                            // errs()<<"最终生成指令: "<<*instruction.getPrevNode()<<"\n";
                            // Instruction *argsInst = instruction.getPrevNode();

                            // 构造函数参数
                            // StringRef filename = instruction.getFunction()->getSubprogram()->getFilename();
                            // int codeline = instruction.getFunction()->getSubprogram()->getLine();
                            // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(filename);
                            // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),codeline,true);
                            // Constant *Constant_curMultiStateNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curMultiStateNum,true);
                            Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(instruction.getContext()),curStateMachineNum,true);
                            
                            int StateType = identifyStateType(loadinst2);
                            if (StateType == -1){
                                continue;
                            }
                            ArrayRef<Value*> funcArgsArray = {loadinst2,Constant_curStateMachineNum};
                            // 插入具体函数
                            int error = 0;
                            InsertFunction(&M,loadinst2,loadinst2,StateMachineScene,StateType,funcArgsArray,error);
                            if (error==0){
                                AlreadyMonitorVarVec.push_back(loadinst2);
                                //导出插桩日志
                                string wholepath = instruction.getFunction()->getSubprogram()->getDirectory().str() + "/" + instruction.getFunction()->getSubprogram()->getFilename().str();
                                addInstrumentInfoToVector(wholepath,instruction.getDebugLoc()->getLine(),instruction.getFunction()->getFunction().getName().str(),delLLVMIRInfoForVar(instruction.getOperand(0)->getName()),StateMachineScene,StateType);
    
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    errs()<<"该函数不具备符合监控的参数\n";
    return false;
}
/**
 * @description: 提取函数参数的语义
 * @param {Value} *arg
 * @return {*} 语义值
 */
StringRef getArgsMeaning(Value *arg,Instruction *instruction,int targetArgPos){
    if (Instruction *instruction = dyn_cast<Instruction>(arg))
    {
        if (!instruction->getName().empty())
        {
            errs()<<"instruction name: "<<instruction->getName()<<"\n";
            return instruction->getName();
        }else{
            errs()<<instruction->getOperand(0)->getName()<<"\n";
            return instruction->getOperand(0)->getName();
        }
    }else{
        Function *function = dyn_cast<Function>(instruction->getOperand(instruction->getNumOperands()-1));
        if (function)
        {
            Value *args = function->getArg(targetArgPos);
            return args->getName();
        }
    }
    assert(1==2);
    return "";
}
/**
 * @description: 检查val是否为i8*类型
 * @param {Value} *val
 * @return {*}
 */
bool checkIfI8Pointer(Value *val){
    if (Instruction *instruction = dyn_cast<Instruction>(val))
    {
        if (PointerType *PT = dyn_cast<PointerType>(instruction->getType()))
        {
            if (IntegerType *IT = dyn_cast<IntegerType>(PT->getPointerElementType()))
            {
                if (IT->getBitWidth() == 8)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
/**
 * @description: check if the args is user defined structure type,now support two types : structure && structure * 
 * @param {Value} *val
 * @return {*}
 */
bool checkIfStructureType(Value *arg){
    if (arg->getType()->isStructTy())
    {
        return true;
    }
    else if (arg->getType()->isPointerTy())
    {
        if (arg->getType()->getPointerElementType()->isStructTy())
        {
            return true;
        }
    }
    return false;
}


bool checkArgsMeaningIfOK_plus(vector<vector<string>> MeanDict,string args,int switchFlag,int &meaningPos){
    for (string &meanval : MeanDict[switchFlag])
    {
        if (int n = args.find(meanval) != string::npos)
        {
            vector<string>::iterator iter = find(MeanDict[switchFlag].begin(),MeanDict[switchFlag].end(),meanval);
            meaningPos = iter - MeanDict[switchFlag].begin();
            return true;
        }
    }
    return false;
}

bool cmp(pair<int,int>&a, pair<int,int>&b) {
	return a.second > b.second;
}

/**
 * @description: iterate GlobalVariables that stored func pointer, then count the used times of func args which type is targetType in these function
 * @param {Module} &M
 * @param {vector<StringRef> FuncPointerVec
 * @param Type *targetType: 目标类型
 * @param unordered_map<int,int> &CounterMap: 统计结果
 * @param vector<StringRef>} &funcVec 记录检测了的函数
 * @return {*}
 */
void iterateGlobalFuncArrayByType(Module &M,vector<StringRef> FuncPointerVec,Type *targetType,unordered_map<int,int> &CounterMap,vector<StringRef> &funcVec){

    for (auto &arrayName : FuncPointerVec)
    {
        // errs()<<"GlobalVariable Name: "<<arrayName<<"\n成员如下：\n";
        Use *operand = M.getNamedGlobal(arrayName)->getInitializer()->getOperandList();
        for (int i = 0; i < M.getNamedGlobal(arrayName)->getInitializer()->getNumOperands(); i++)
        {
            Function *func = dyn_cast<Function>(operand[i].get());
            if (func != NULL && !func->isDeclaration())
            {
                // if element is user defined function
                // errs()<<"["<<i<<"]\t"<<func->getName()<<"\n";
                funcVec.push_back(func->getName());
                //TODO:照下面逻辑来就行，直接处理
            }
            else
            {
                // if element is structure rather than user defined function, then iterate the member value of the structure, see whether it contain function pointer
                Constant *gv = dyn_cast<Constant>(operand[i].get());

                if (gv != NULL && gv->getNumOperands() > 0)
                {
                    // If getNumOperands()>0 ——> structure ——> so iterate the member value
                    for (Use &U : gv->operands())
                    {
                        Function *func = dyn_cast<Function>(U.get());
                        // if one member is auser defined function ——> count the number of the args that function used
                        if (func != NULL && !func->isDeclaration())
                        {
                            // errs()<<"["<<i<<"]\t"<<func->getName()<<"\n";
                            funcVec.push_back(func->getName());
                            checkFuncArgUse(func,targetType,CounterMap);
                        }
                    }
                }
            }
        }
    }
}

/**
 * @description: count the used times of func args which type is targetType in these function
 * @param {Function} *function 检查函数
 * @param {Type} *targetType 目标类型
 * @param {unordered_map<int,int>} &CounterMap 统计结果
 * @return {*}
 */
void checkFuncArgUse(Function *function, Type *targetType,unordered_map<int,int> &CounterMap){
    // 函数参数类型是否是目标类型，如果是则进一步遍历这个Function
    // int argsize = function->arg_size();
    
    for (auto arg=function->args().begin();arg!=function->args().end();arg++)
    {
        // errs()<<"函数"<<function->getName()<<"参数个数有"<<function->arg_size()<<"\n";
        // errs()<<function->args().begin()<<"\t"<<function->args().end()<<"\n";
        // errs()<<"i:"<<i<<"\t终止条件:"<<function->arg_size()<<"\t比较结果:"<<(i<function->arg_size())<<"\n";
        // auto *arg = function->getArg(i);
        if (arg->getType()==targetType)
        {
            // errs()<<"\t参数:"<<*arg<<"\t"<<arg->getName()<<"\t"<<*arg->getType()<<"\t待比较的类型: "<<*targetType<<"\n";
            for (BasicBlock &basicblock : *function)
            {
                for (Instruction &instruction : basicblock)
                {
                    if (GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(&instruction))
                    {
                        if (gepinst->getOperand(0)->getType()==targetType && gepinst->getNumOperands()==3)  //I find structure type's gepinst has 3 opearands, but array[] may be only 2
                        {
                            // errs()<<"GetElementPtrInst: "<<*gepinst<<"\t"<<*gepinst->getOperand(1)<<"\t"<<*gepinst->getOperand(2)<<"\n";
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(gepinst->getOperand(2)))
                            {
                                if (CI->getBitWidth()==32)
                                {
                                    int pos = CI->getSExtValue();
                                    CounterMap[pos]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
/**
 * @description: 检查当前指令是否是Gep指令，且操作类型是targetType，操作数1是FirstGepPos，操作数个数是GepOpearandNum
 * @param {Instruction} *instruction
 * @param {Type} *targetType
 * @param {int} FirstGepPos
 * @param {int} GepOpearandNum
 * @return {*}
 */
bool checkFirstGepInst(Instruction *instruction,Type *targetType,int FirstGepPos,int GepOpearandNum){
    if (GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(instruction))
    {
        if (gepinst->getOperand(0)->getType() == targetType && GepOpearandNum==3)
        {
            ConstantInt *CI = dyn_cast<ConstantInt>(gepinst->getOperand(2));
            if (CI->getBitWidth()==32 && CI->getSExtValue()==FirstGepPos)
            {
                return true;
            }
        }
    }
    return false;
}
/**
 * @description: 针对structure**类型，遍历FuncPointerVec中的函数，统计指定类型的Gep指令的下一个（第二个）Gep指令的操作数value使用确定，返回最多使用的那个pos
 * @param {Module} &M
 * @param {vector<StringRef>} FuncPointerVec 待检测函数列表
 * @param {Type} *targetType First Gep指令的操作类型
 * @param {int} FirstGepPos First Gep指令的操作数
 * @return {*}
 */
int findSecondGepOperandVal(Module &M,vector<StringRef> FuncVec,Type *targetType,int FirstGepPos){
    unordered_map<int,int> CounterMap;
    vector<pair<int,int>> CounterMapVec;

    for (auto &arrayName : FuncVec)
    {
        Function *func = M.getFunction(arrayName);
        // errs()<<arrayName<<"\t"<<func->getName()<<"\n";
        for (auto &basicblock : *func)
        {
            for (auto &instruction :basicblock)
            {
                if (checkFirstGepInst(&instruction,targetType,FirstGepPos,3))
                {
                    //找到第一个符合标准的GetElementInstPrt,向后遍历找到下一个GetElementInstPtr,统计个数
                    //从这个指令的下一条开始找，直到找到一个符合的GEP指令，当然找不到就算了
                    findTheNextGepFromCurInst(&instruction,CounterMap,2);
                }
            }
        }
    }
    for (auto it : CounterMap)
    {
        CounterMapVec.push_back(it);
    }
    std::sort(CounterMapVec.begin(),CounterMapVec.end(),cmp);
    if (CounterMapVec.size()!=0)
    {
        int maxValPos = CounterMapVec[0].first;
        return maxValPos;
    }
    return -1;
}

/**
 * @description: 从当前Gep指令开始寻找下一个符合目标类型的Gep指令
 * @param {Instruction} *instruction
 * @param {unordered_map<int,int>} &CounterMap
 * @param {int} GepOperandNum
 * @return {*}
 */
void findTheNextGepFromCurInst(Instruction *instruction,unordered_map<int,int> &CounterMap,int GepOperandNum){
    for (auto *user : instruction->users()){
        if (!isa<GetElementPtrInst>(user))
        {
            for (auto *useruser : user->users())
            {
                if (isa<GetElementPtrInst>(useruser) && useruser->getNumOperands()==GepOperandNum && !isa<Instruction>(useruser->getOperand(GepOperandNum-1)))
                {
                    ConstantInt *constint = dyn_cast<ConstantInt>(useruser->getOperand(GepOperandNum-1));
                    if ((constint->getBitWidth()==32 || constint->getBitWidth()==64))
                    {
                        int val = constint->getSExtValue();
                        CounterMap[val]++;
                    }
                }
            }
        }
    }
}
/**
 * @description: check if the type is i8*
 * @param {Type} *type
 * @return {*}
 */
bool checkTypeIfi8pointer(Type *type){
    if (PointerType *PT = dyn_cast<PointerType>(type))
    {
        if (IntegerType *IT = dyn_cast<IntegerType>(PT->getPointerElementType()))
        {
            if (IT->getBitWidth() == 8)
            {
                return true;
            }
        }
    }
    return false;
}
/**
 * @description: check if the type is i8**
 * @param {Type} *type
 * @return {*}
 */
bool checkTypeIfi8doublepointer(Type *type){
    if (PointerType *ptr = dyn_cast<PointerType>(type))
    {
        if (PointerType *ptr2 = dyn_cast<PointerType>(ptr->getElementType()))
        {
            if (IntegerType *IT = dyn_cast<IntegerType>(ptr2->getPointerElementType()))
            {
                if (IT->getBitWidth() == 8)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
// create Gep->Load->Gep->Load Inst for the structure** variable
/*
    args : 
        1.srcInst: 作为第一条Gep的操作数
        2.targetInst: 插桩位置的指令
        3.FirstGepType: 第一个Gep的操作类型
        4.FirstPos: 第一个Gep的第二个索引，第一个索引默认取0
        5.SecondPos: 第二个Gep的索引
*/
// bool createGepInstForStruct2Pointer(Instruction *srcInst,Instruction *targetInst,Type *FirstGepType,int FirstPos,int SecondPos){
//     Value *Gep1_indexList[2]= {ConstantInt::get(Type::getInt32Ty(targetInst->getParent()->getContext()),0),ConstantInt::get(Type::getInt32Ty(targetInst->getParent()->getContext()),FirstPos)};
//     Value *Gep2_indexList[1]= {ConstantInt::get(Type::getInt32Ty(targetInst->getParent()->getContext()),0)};

//     IRBuilder <>IRB(targetInst->getParent());
//     IRB.SetInsertPoint(targetInst);

//     IRB.CreateInBoundsGEP(FirstGepType,srcInst,ArrayRef<Value*>(Gep1_indexList,2),"autoGep1");
//     IRB.CreateLoad(targetInst->getPrevNode(),"autoLoad1");
//     IRB.CreateInBoundsGEP(Type::getInt8PtrTy(targetInst->getParent()->getContext()),targetInst->getPrevNode(),ArrayRef<Value*>(Gep2_indexList,1),"autoGep2");
//     IRB.CreateLoad(targetInst->getPrevNode(),"autoLoad2");
//     errs()<<"最终生成指令: "<<*srcInst->getPrevNode()<<"\n";

//     Instruction *targetPrev = targetInst->getPrevNode();
//     Module *M = targetInst->getModule();
//     FunctionCallee statelog_char = M->getOrInsertFunction(StringRef("make_statelog_sensitive_for_char"),FunctionType::get(Type::getVoidTy(M->getContext()),{Type::getInt8PtrTy(M->getContext()),Type::getInt8PtrTy(M->getContext())},false));
    
//     char *prefix = "xiangpuTest";//FIXME:修改为代码行号...

//     IRBuilder <>IRB2(targetInst->getParent());
//     Value *prefixValue = IRB2.CreateGlobalStringPtr(prefix);
//     IRB2.SetInsertPoint(targetInst);
//     CallInst *cal_statelog = IRB2.CreateCall(statelog_char,{prefixValue,targetPrev});
//     //TODO:待续
//     return true;
// }

/**
 * @description: 遍历Module中的StructTypes,检查StructTypes中是否存在成员为FunPointerArrayElementTypeStringVec或FunPointerArrayElementTypeValueVec中的成员,有则更新两个数组
 * @param {Module} &M
 * @param {vector<string> &FunPointerArrayElementTypeStringVec: 保存具有目标类型的结构体字符串名称，用于检查Union时与dwarf信息交互
 * @param vector<Type*>} &FunPointerArrayElementTypeValueVec：保存具有目标类型的结构体IR地址，用于非Union类型的检查
 * @return {*}
 */
void extractGlobalStructContainsTargetType(Module &M,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec){
    TypeFinder StructTypes;
    StructTypes.run(M,true);
    // *Iterate all struct types in the module
    for (auto *STy : StructTypes)
    {
        // Iterate struct member to check if it's the target Type which is in FunPointerArrayElementTypeValueVec
        for (size_t i = 0; i < STy->getNumElements(); i++)
        {
            // if this member type is structure *
            if (PointerType *pointer = dyn_cast<PointerType>(STy->getElementType(i)))
            {
                if (pointer->getElementType()->isStructTy())
                {
                    // check the structure if already in FunPointerArrayElementTypeValueVec (FunPointerArrayElementTypeValueVec存储了全局数组的基本元素类型)
                    if (checkStructMemberIfContainsTargetType(pointer->getElementType(),FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec))
                    {
                        storeStructTypeAndStringName(STy->getStructName(),STy,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
                        break;
                    }
                }
            }else if (STy->getElementType(i)->isStructTy()){
                // if this member type is structure,including union and structure
                if (STy->getElementType(i)->getStructName().startswith("union."))
                {
                    // //union type -> 判断当前结构体其他成员是否已经被识别到了，如果是，则不再分析union；否则，继续
                    // if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),STy->getElementType(i))!=FunPointerArrayElementTypeValueVec.end())
                    // {
                    //     // errs()<<"尽管结构体中有UNION，但其他成员已经有目标类型了，所在不再判断\n";
                    //     break;
                    // }else{
                    //     // errs()<<"结构体中有UNION，需调用DwarfInfo结合分析结构体:"<<STy->getStructName()<<"\n";
                        
                    //     int res = checkUnionIfContainsTargetType(M,STy->getStructName().substr(7),i,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
                    //     if (res)
                    //     {
                    //         // errs()<<"分析DwarfInfo后，得出当前结构体有目标类型\n";
                    //         break;
                    //     }
                    // }

                }else{
                    //structure type -> 1.已经在Vector中有->查询成功 2.Vector中没有，调用函数单独向下一层去查找这个结构体的各成员
                    if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),STy->getElementType(i))!=FunPointerArrayElementTypeValueVec.end())
                    {
                        errs()<<"<查询成功!> 当前结构体内有目标类型: "<<*STy<<"\tName: "<<STy->getStructName()<<"\n";
                        // FunPointerArrayElementTypeValueVec.push_back(STy);
                        storeStructTypeAndStringName(STy->getElementType(i)->getStructName(),STy->getElementType(i),FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
                        break;
                    }else if (checkStructMemberIfContainsTargetType(STy->getElementType(i),FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec))
                    {
                        errs()<<"<查询成功!> 当前结构体内有目标类型: "<<*STy<<"\tName: "<<STy->getStructName()<<"\n";
                        // FunPointerArrayElementTypeValueVec.push_back(STy);
                        storeStructTypeAndStringName(STy->getElementType(i)->getStructName(),STy->getElementType(i),FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
                        break;
                    } 
                }
            }
        }
    }
}
/**
 * @description: 符合标准的目标类型存储至两数组中
 * @param {StringRef} structName
 * @param {Type} *structType
 * @param {vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*>} &FunPointerArrayElementTypeValueVec
 * @return {*}
 */
void storeStructTypeAndStringName(StringRef structName,Type *structType,vector<string> &FunPointerArrayElementTypeStringVec,vector<Type*> &FunPointerArrayElementTypeValueVec){
    if (structName.startswith("struct."))
    {
        FunPointerArrayElementTypeStringVec.push_back(structName.substr(7).str());
        FunPointerArrayElementTypeStringVec.push_back(structName.str());
        FunPointerArrayElementTypeValueVec.push_back(structType);
    }
    // unique the FunPointerArrayElementTypeStringVec
    set<string>s(FunPointerArrayElementTypeStringVec.begin(),FunPointerArrayElementTypeStringVec.end());
    FunPointerArrayElementTypeStringVec.assign(s.begin(),s.end());
    // unique the FunPointerArrayElementTypeValueVec
    set<Type*>ss(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end());
    FunPointerArrayElementTypeValueVec.assign(ss.begin(),ss.end());
}
/**
 * @description: 检查struct成员是否有目标类型
 * @param {Type} *type
 * @param {vector<string> FunPointerArrayElementTypeStringVec,
 * @param vector<Type*>} FunPointerArrayElementTypeValueVec
 * @return {*}
 */
bool checkStructMemberIfContainsTargetType(Type *type,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec){
    StructType *structType = dyn_cast<StructType>(type);
    if (!structType)
    {
        return false;
    }
    for (size_t i = 0; i < structType->getNumElements(); i++)
    {
        // if member type is structure*
        if (PointerType *pointer = dyn_cast<PointerType>(structType->getElementType(i)))
        {
            if (pointer->getElementType()->isStructTy())
            {
                if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),pointer->getElementType())!=FunPointerArrayElementTypeValueVec.end())
                {
                    return true;
                }
            }
        }else if (structType->getElementType(i)->isStructTy())
        {
            // if this member type is union, then handle it by dwarf info
            if (structType->getElementType(i)->getStructName().startswith("union."))
            {
                if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),structType->getElementType(i))!=FunPointerArrayElementTypeValueVec.end())
                {
                    return true;
                }else{
                    // errs()<<"结构体中有UNION，需调用DwarfInfo结合分析\n"; 不套娃，这里不再检查
                    return false;
                }
            }else{
                //if this member type is struct
                if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),structType->getElementType(i))!=FunPointerArrayElementTypeValueVec.end()){
                    return true;
                }
            }
        }
    }
    return false;
}
/**
 * @description: 检查Union成员中是否包含目标类型
 * @param {Module} &M
 * @param {StringRef} structName
 * @param {int} unionPos
 * @param {vector<string> FunPointerArrayElementTypeStringVec,vector<Type*>} FunPointerArrayElementTypeValueVec
 * @return {*}
 */
bool checkUnionIfContainsTargetType(Module &M,StringRef structName,int unionPos,vector<string> FunPointerArrayElementTypeStringVec,vector<Type*> FunPointerArrayElementTypeValueVec){
    DebugInfoFinder Finder;
    Finder.processModule(M);
    for (auto &op : Finder.types())
    {
        // find the target structure （Tag:DW_TAG_Structure_Type）
        if (op->getTag()==19 && op->getName().equals(structName)) 
        {
            DICompositeType *dicom = dyn_cast<DICompositeType>(op);
            if (dicom)
            {
                DINodeArray diarray = dicom->getElements();
                // get the target union member
                DIDerivedType *dicom_op = dyn_cast<DIDerivedType>(diarray[unionPos]);
                if (dicom_op && dicom_op->getBaseType()->getTag()==23)
                {
                    DICompositeType *dicom_union = dyn_cast<DICompositeType>(dicom_op->getBaseType());
                    if (dicom_union)
                    {
                        // errs()<<"dicom_union: "<<*dicom_union<<"\tTag: "<<dicom_union->getTag()<<"\tElements: "<<dicom_union->getElements().size()<<"\n";
                        //<0x81dc128> = distinct !DICompositeType(tag: DW_TAG_union_type, scope: <0x81daf38>, file: <0x81bb1e0>, line: 43, size: 64, elements: <0x81c65c8>)
                        // iterate union member type
                        DINodeArray unionArray = dicom_union->getElements();
                        for (size_t i = 0; i < unionArray.size(); i++)
                        {
                            DIDerivedType *dicom_union_op = dyn_cast<DIDerivedType>(unionArray[i]);
                            // errs()<<"member "<<i<<" "<<*dicom_union_op<<"\tbasetype: "<<*dicom_union_op->getBaseType()<<"\tbasetype tag: "<<dicom_union_op->getBaseType()->getTag()<<"\n";
                            //if the basetype of member is pointer
                            if (dicom_union_op && dicom_union_op->getBaseType()->getTag() == 15)
                            {
                                DIDerivedType *pointer = dyn_cast<DIDerivedType>(dicom_union_op->getBaseType());
                                if (pointer && iterateUnionToCheckIfContainTargetStruct(pointer,FunPointerArrayElementTypeStringVec))
                                {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}
/**
 * @description: 递归检查Union的成员类型是否有目标Type的
 * @param {DIDerivedType} *curType
 * @param {vector<string>} FunPointerArrayElementTypeVec
 * @return {*}
 */
bool iterateUnionToCheckIfContainTargetStruct(DIDerivedType *curType,vector<string> FunPointerArrayElementTypeVec){
    // If the curType is not structure and the curType has BaseType() ,then obtain the BaseType and check it（note: Structure Tag == 19）
    if (curType->getTag()!=19 && curType->getBaseType())
    {
        DIDerivedType *nextType = dyn_cast<DIDerivedType>(curType->getBaseType());
        if (!nextType)
        {
            DICompositeType *Final = dyn_cast<DICompositeType>(curType->getBaseType());
            if (Final && Final->getTag()==19)
            {
                string structName = Final->getName().str();
                if (find(FunPointerArrayElementTypeVec.begin(),FunPointerArrayElementTypeVec.end(),structName)!=FunPointerArrayElementTypeVec.end())
                {
                    return true;
                }
            }
            return false;
        }
        return iterateUnionToCheckIfContainTargetStruct(nextType,FunPointerArrayElementTypeVec);
    }
    return false;
}
/**
 * @description: 检查type是否存在于FunPointerArrayElementTypeValueVec中，若存在，则将函数名称存入FuncUsedTargetStructVec
 * @param {Function} *function
 * @param {Type} *type
 * @param {vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef>} &FuncUsedTargetStructVec
 * @return {*}
 */
void checkTypeIfInArray(Function *function,Type *type,vector<Type*> &FunPointerArrayElementTypeValueVec,vector<StringRef> &FuncUsedTargetStructVec){
    if (find(FunPointerArrayElementTypeValueVec.begin(),FunPointerArrayElementTypeValueVec.end(),type) != FunPointerArrayElementTypeValueVec.end())
    {
        FuncUsedTargetStructVec.push_back(function->getName());
    }
    //去重
    set<StringRef>ss(FuncUsedTargetStructVec.begin(),FuncUsedTargetStructVec.end());
    FuncUsedTargetStructVec.assign(ss.begin(),ss.end());
}

void handleCandidateObjForFuncPointer(Module &M,vector<vector<string>> MeaningDict,vector<StringRef> FuncPointerVec){
    /*
        1.检查不同类型的有几个 -> the number of type
            1.若number==1 && 其具体数量有1个:
                1.situation2 -> 监控数组的索引
                2.situation3 -> 检查函数参数进行判断，决定具体监控哪个
            2.其他:
                1.NLP语义筛选对象，IF 长度 == 1 -> 处理
                2.否则 -> assert(1==2)  checkArgsMeaningIfOK
    */
    errs()<<"<CandidateObjForFuncPointer> size: "<<CandidateObjForFuncPointer.size()<<"\n";
    if (CandidateObjForFuncPointer.size() == 0){
        return;
    }

    CandidateObj *candidateObj = NULL;
    if (CandidateObjForFuncPointer.size()==1)
    {
        candidateObj = CandidateObjForFuncPointer[0];
    }else{
        //NLP判断函数的语义
        for (auto one : CandidateObjForFuncPointer)
        {
            if (checkArgsMeaningIfOK(MeaningDict,one->function->getName().lower(),Handle_Func_Pos,0))
            {
                candidateObj = CandidateObjForFuncPointer[0];
                break;
            }
        }
    }
    errs()<<"找到候选的:"<<*candidateObj->callInst<<"\t"<<candidateObj->function->getFunction().getName()<<"\n";
    if (candidateObj->situation == FuncPointer2)
    {
        // 上升basicblock , 监控数组的索引
        Instruction *inst = extractArrayIdx(candidateObj->callInst->getParent(),FuncPointerVec);

        // 构造函数参数
        IRBuilder <>IRB(inst->getParent());
        // StringRef filename = inst->getFunction()->getSubprogram()->getFilename();
        // int codeline = inst->getFunction()->getSubprogram()->getLine();
        // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(filename);
        // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(inst->getContext()),codeline,true);
        Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(inst->getContext()),curStateMachineNum,true);

        int StateType = identifyStateType(inst);
        if (StateType == -1){
            return;
        }
        ArrayRef<Value*> funcArgsArray = {inst,Constant_curStateMachineNum};
        //插入具体函数
        int error = 0;
        InsertFunction(&M,inst,inst,StateMachineScene,StateType,funcArgsArray,error);
        if (error==0){
            AlreadyMonitorVarVec.push_back(inst);
            //导出插桩日志
            string wholepath = inst->getFunction()->getSubprogram()->getDirectory().str() + "/" + inst->getFunction()->getSubprogram()->getFilename().str();
            addInstrumentInfoToVector(wholepath,inst->getDebugLoc()->getLine(),inst->getFunction()->getName().str(),delLLVMIRInfoForVar(inst->getOperand(0)->getName()),StateMachineScene,StateType);
        }
    }else if (candidateObj->situation == FuncPointer3)
    {
        Instruction *instruction = candidateObj->callInst;
        // 检查函数的参数进行判断，决定具体监控哪一个
        int results = checkFuncArgsMeaning(M,*instruction,MeaningDict,FuncPointerVec);
        errs()<<"results === "<<results<<"\n";
        if (results)
        {
            //TODO:插桩OK,打印日志等信息
            
        }
    }
}

Instruction* extractArrayIdx(BasicBlock *basicblock,vector<StringRef> FuncPointerVec){
    for (Instruction &instruction : *basicblock)
    {
        if (GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(&instruction))
        {
            if (find(FuncPointerVec.begin(),FuncPointerVec.end(),gepInst->getOperand(0)->getName()) != FuncPointerVec.end())
            {
                for (auto &operand : gepInst->operands())
                {
                    Instruction *inst = dyn_cast<Instruction>(operand);
                    if (inst)
                    {
                        return inst;
                    }
                }
            }
        }
    }
    assert(1==2);
}