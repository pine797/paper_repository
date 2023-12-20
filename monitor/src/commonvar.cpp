#include "callgraph.h"
#include "funcpointer.h"
#include "commonvar.h"
#include "globalvar.h"
// #include "switchmodule.h"

using namespace std;
using namespace llvm;

extern int curStateMachineNum;
extern int curSubStateNum;

extern vector<Value*> AlreadyMonitorVarVec;

extern StateMachineVarAssistInfo stateMachineAssistInfo;

void backtraceToGetStructureType(Instruction *instruction,map<Type*,int> &resultVec){
    BasicBlock *basicblock = instruction->getParent();
    //backtrace N level
    queue<vector<Instruction*>> Iqueue;
    vector<Instruction*> tmp;
    for (auto &one : instruction->operands())
    {
        if (Instruction *opinst = dyn_cast<Instruction>(one))
        {
            if (opinst->getParent() == basicblock)
            {
                tmp.push_back(opinst);
            }
        }
    }
    Iqueue.push(tmp);

    int level = 0;
    while (level < 10 && !Iqueue.empty())
    {
        vector<Instruction*> tmp = Iqueue.front();
        Iqueue.pop();
        level++;
        backtraceToGetStructureType_onelevel(basicblock,tmp,Iqueue,resultVec);
    }
}

void backtraceToGetStructureType_onelevel(BasicBlock *basicblock,vector<Instruction*> &tmp,queue<vector<Instruction*>> &Iqueue,map<Type*,int> &resultVec){
    vector<Instruction*> backInstVec;
    for (auto instruction : tmp)
    {
        for (auto &one : instruction->operands())
        {
            if (Instruction *opinst = dyn_cast<Instruction>(one))
            {
                if (opinst->getParent() == basicblock)
                {
                    if (GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(opinst))
                    {
                        // errs()<<"回溯得到: "<<*gepInst<<"\ttype: "<<*gepInst->getSourceElementType()<<"\n";
                        resultVec[gepInst->getSourceElementType()]++;
                    }else{
                        backInstVec.push_back(opinst);
                    }
                }
            }
        }
    }
    if (backInstVec.size()!=0)
    {
        Iqueue.push(backInstVec);
    }
}

// void countAllStructureUsing(Module &M,map<Function*,int> CallGraphMap){
void countAllStructureUsing(Module &M){
    
    map<Type*,int> countResultVec;

    for (auto &function : M.getFunctionList())
    {
        // if (CallGraphMap.find(&function) == CallGraphMap.end())
        //     continue;
        // if (!function.getName().equals(StringRef("handle__packet")))
        //     continue;
        

        for (inst_iterator iterator = inst_begin(&function);iterator != inst_end(&function);iterator++)
        {
            Instruction *instruction = &*iterator;
            // checkInstructionIfIF(instruction);

            if (SwitchInst *swinst = dyn_cast<SwitchInst>(instruction))
            {
                if (Instruction *condition = dyn_cast<Instruction>(swinst->getCondition()))
                {
                    // errs()<<"待处理对象: "<<*condition<<"\n";
                    backtraceToGetStructureType(condition,countResultVec);
                }
            }
        }
    }
    //
    errs()<<"countResultVec size: "<<countResultVec.size()<<"\n";
    for (auto one : countResultVec)
    {
        errs()<<*one.first<<"\t"<<one.second<<"\n";
    }   
}

StringRef backtraceForMeaning(Instruction *instruction,BasicBlock *constrBasicblock){
    //回溯找第一条load指令，取其name
    queue<vector<Instruction*>> Iqueue;
    vector<Instruction*> tmp;
    for (auto &one : instruction->operands())
    {
        if (Instruction *opinst = dyn_cast<Instruction>(one))
        {
            LoadInst *loadinst = dyn_cast<LoadInst>(opinst);

            if (opinst->getParent() == constrBasicblock && !loadinst)
            {
                tmp.push_back(opinst);
            }else if (opinst->getParent() == constrBasicblock && loadinst)
            {
                return loadinst->getOperand(0)->getName();
            }
        }
    }
    Iqueue.push(tmp);

    int level = 0;
    while (level < 10 && !Iqueue.empty())
    {
        vector<Instruction*> tmp = Iqueue.front();
        vector<Instruction*> tmp_inner;
        Iqueue.pop();
        level++;
        
        for (auto instruction : tmp)
        {
            for (auto &one : instruction->operands())
            {
                if (Instruction *opinst = dyn_cast<Instruction>(one))
                {
                    LoadInst *loadinst = dyn_cast<LoadInst>(opinst);

                    if (opinst->getParent() == constrBasicblock && !loadinst)
                    {
                        tmp_inner.push_back(opinst);
                    }else if (opinst->getParent() == constrBasicblock && loadinst)
                    {
                        return loadinst->getOperand(0)->getName();
                    }
                }
            }
        }
        if (tmp_inner.size()!=0)
        {
            Iqueue.push(tmp_inner);
        }
    }
    return "";
}
/**
 * @description: 处理LLVM IR的变量名称，删除末尾的数字
 * @param {StringRef} str
 * @return {*}
 */
StringRef processVariableNameWithoutNum(StringRef str){
    int i;
    for (i = str.size()-1; i >=0 ; i--)
    {
        if (!(str[i]>='0'&&str[i]<='9'))
        {
            break;
        }
    }
    return str.substr(0,i+1);
}

/**
 * @description: 检查instruction是否是IF的条件表达式
 * @param {Instruction} *instruction
 * @return {*}
 */
bool checkInstructionIfIF(Instruction *instruction,vector<IfConditionObj> AllIfObjInCurFunc){
    BasicBlock *curBasicBlock = instruction->getParent();

    for (size_t i = 0; i < AllIfObjInCurFunc.size(); i++)
    {
        IfConditionObj curObj = AllIfObjInCurFunc[i];
        if (find(curObj.conditionBBVec.begin(),curObj.conditionBBVec.end(),curBasicBlock) != curObj.conditionBBVec.end())
        {

            goto CheckCondition;
        }
    }
    return false;
    
CheckCondition:

    Instruction *lastInst = &*curBasicBlock->end()->getPrevNode();//br

    if (!(isa<BranchInst>(lastInst) && lastInst->getNumOperands()==3))
    {
        return false;
    }

    BasicBlock::iterator iter(instruction);
    for (; iter != curBasicBlock->end(); iter++)
    {
        Instruction *curInst = &*iter;
        if (isa<ICmpInst>(curInst))
        {
            break;
        }   
    }
    if (iter==curBasicBlock->end())
    {
        return false;
    }
    
    Instruction *icmpInst = &*iter;
    Instruction *branchInst = &*iter->getNextNode();
    if (isa<BranchInst>(branchInst) && branchInst->getNumOperands()==3)
    {   
        //回溯icmp指令操作数，判断是否与instruction有关
        if (checkInstructionIfIcmpCondition(curBasicBlock,instruction,icmpInst))
        {
            if (checkBBSuccessorIfStartWithTargetStringref(curBasicBlock,StringRef("if.")))
            {
                return true;
            }else{
                for (auto *one : successors(curBasicBlock))
                {
                    if (checkBBSuccessorIfStartWithTargetStringref(one,StringRef("if.")))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool checkBBSuccessorIfStartWithTargetStringref(BasicBlock *curbasicblock,StringRef targetString){
    for (auto *one : successors(curbasicblock))
    {
        if (one->getName().startswith(targetString))
        {
            return true;
        }
    }
    return false;
}
/**
 * @description: 检查目标Instruction是否位于ICMP的条件表达式中，从sourceInst（条件表达式）开始遍历其操作数，向上回溯，匹配是否==instruction
 * @param {BasicBlock} *curBasicBlock
 * @param {Instruction} *instruction
 * @param {Instruction} *sourceInst
 * @return {*}
 */
bool checkInstructionIfIcmpCondition(BasicBlock *curBasicBlock,Instruction *instruction,Instruction *sourceInst){
    vector<Instruction*> tmp;
    queue<vector<Instruction*>> fqueue;
    // errs()<<"\ntarget inst: "<<*instruction<<"\n";
    // errs()<<"icmpInst1: "<<*sourceInst<<"\n";
    for (auto &op : sourceInst->operands())
    {
        Instruction *opInst = dyn_cast<Instruction>(op);
        if (opInst!=NULL && opInst->getParent()==curBasicBlock)
        {
            // errs()<<"op1: "<<*op<<"\n";
            if (opInst == instruction)
            {
                return true;
            }else{
                tmp.push_back(opInst);
            }
        }
    }
    fqueue.push(tmp);
    // errs()<<"tmp size: "<<tmp.size()<<"\tqueue size: "<<fqueue.size()<<"\n";

    while (fqueue.size()!=0)
    {
        vector<Instruction*> tmpp = fqueue.front();
        vector<Instruction*> tmppp;
        fqueue.pop();

        // errs()<<"tmpp size: "<<tmpp.size()<<"\n";
        
        for (size_t i = 0; i < tmpp.size(); i++)
        {
            // errs()<<"icmpInst2: "<<*tmpp[i]<<"\n";

            for (auto &op : tmpp[i]->operands())
            {
                Instruction *opInst = dyn_cast<Instruction>(op);
                if (opInst!=NULL && opInst->getParent()==curBasicBlock)
                {
                // errs()<<"opInst2: "<<*op<<"\tequal?: "<<(op==instruction)<<"\n";
                    if (opInst == instruction)
                    {
                        return true;
                    }else if (opInst->getParent()==curBasicBlock)
                    {
                        tmppp.push_back(opInst);
                    }
                }
            }
        }
        if (tmppp.size()!=0)
        {
            fqueue.push(tmppp);
        }else{
            return false;
        }
    }
    return false;
}

// 查询inst来自哪一个IF Obj
int findInstFromWhichIFObj(vector<IfConditionObj> AllIfObjInCurFunc,Instruction* instruction){
    BasicBlock *curBasicBlock = instruction->getParent();

    for (size_t i = 0; i < AllIfObjInCurFunc.size(); i++)
    {
        IfConditionObj curObj = AllIfObjInCurFunc[i];
        if (find(curObj.conditionBBVec.begin(),curObj.conditionBBVec.end(),curBasicBlock) != curObj.conditionBBVec.end())
        {
            return i;
        }
    }
    return -1;
}

/**
 * @description: 遍历Module所有Load指令，重点关注位于IF和Switch条件表达式的Load，扫描后逐一处理
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,vector<vector<string>>} MeaningDict
 * @return {*}
 */
void iterateAllSwAndIfCondition(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict){
    for (auto &function : M.getFunctionList())
    {
        if (CallGraphMap.find(&function) == CallGraphMap.end())
        {
            continue;
        } 

        #ifdef DEBUG
        errs()<<"Cur function: "<<function.getName()<<"\n";
        #endif
        
        // 预先提取IFCondition：将Function内的IF对象全部提取出来
        vector<IfConditionObj> AllIfObjInCurFunc;
        extractIFObjInCurFunction(&function,AllIfObjInCurFunc);

        map<string,vector<Instruction*>> candidateResultsMap;

        map<int,vector<string>> assistMap;   //辅助 “避免出现if(state==1 || state ==2)，插桩两次的情况” 的Map

        //遍历所有Load instruction，如果类型为i8、i32、i64，且变量语义与协议有关，则判断其在哪一个IF/Switch中出现，然后插桩监控
        for (inst_iterator iterator = inst_begin(&function);iterator != inst_end(&function);iterator++)
        {
            Instruction *instruction = &*iterator;
            // 全局扫描 “普通类型的关键变量”：关注switch和IF条件表达式中的变量
            if (LoadInst *loadinst = dyn_cast<LoadInst>(instruction))
            {
                Value *variable = loadinst->getOperand(0);  //load加载的对象 的 Value
                Type *variable_type = variable->getType();  //load加载的对象 的 Type
                Instruction *variable_inst = dyn_cast<Instruction>(variable);  //load加载的对象的 Instruction 
                if (!variable_inst)
                {
                    continue;
                }
                //只关注i8*、i32*、i64*
                if (PointerType *pointertype = dyn_cast<PointerType>(variable_type))
                {
                    Type *varElementType = pointertype->getElementType();
                    if (varElementType->isIntegerTy(8) || varElementType->isIntegerTy(32) || varElementType->isIntegerTy(64))
                    {
                        // StringRef variableName = delLLVMIRInfoForVar(variable->getName());
                        string variableName = getVariableNameWithParent(variable);
                        // errs()<<"variableName: "<<variableName<<"\n";
                        // 如果variable是getelementptr成员变量，有粒度大的问题，例如msg->state和handle->state，都是state，但区别很大。
                        
                        int meanPos;
                        //检查变量名称是否符合标准，返回其匹配成功的语义下标
                        if (checkArgsMeaningIfOK(MeaningDict,variableName,Protocol_Args_Pos,0) || checkArgsMeaningIfOK(MeaningDict,variableName,StateMachine_Args_Pos,1))
                        // if (checkArgsMeaningIfOK_plus(MeaningDict,variableName.lower(),2,meanPos))
                        {
                            // 检查variable_inst来源于一个IF Obj，避免出现if(state==1 || state ==2)，插桩两次的情况
                            int ifObjPos = findInstFromWhichIFObj(AllIfObjInCurFunc,loadinst);
                            if (ifObjPos == -1)
                            {
                                continue;
                            }
                            
                            int ifObjPtr = AllIfObjInCurFunc[ifObjPos].IfObjNumber;
                            // errs()<<"variable_inst: "<<*loadinst<<"\tifObjPos "<<ifObjPos<<"\tifObjPtr: "<<ifObjPtr<<"\n";

                            map<int,vector<string>>::iterator ifObjIter = assistMap.find(ifObjPtr);
                            if (ifObjIter == assistMap.end())    //未存储过 -> 存储ifOBJ
                            {
                                vector<string> stringVec;
                                stringVec.push_back(variableName);
                                assistMap.insert(pair<int,vector<string>>(ifObjPtr,stringVec));
                            }else{
                                //存储过，检查ifObj 是否已经保存过当前变量
                                vector<string>::iterator stringIter = find(ifObjIter->second.begin(),ifObjIter->second.end(),variableName);
                                if (stringIter == ifObjIter->second.end())
                                {
                                    ifObjIter->second.push_back(variableName);
                                }else{
                                    //保存过，则跳过
                                    // errs()<<"跳过不保存了\n";
                                    continue;
                                }
                            }
                            
                            // errs()<<"是否在If条件表达式中: "<<checkInstructionIfIF(loadinst)<<"\n";
                            map<string,vector<Instruction*>>::iterator iter = candidateResultsMap.find(variableName);
                            if (iter == candidateResultsMap.end())
                            {
                                //第一次插入
                                vector<Instruction*> tmpvec;
                                tmpvec.push_back(instruction);
                                candidateResultsMap.insert(pair<string,vector<Instruction*>>(variableName,tmpvec));
                            }else{
                                iter->second.push_back(instruction);
                            }
                        }
                    }
                }
            }
        }
        //处理当前Function内的候选目标
        handleCandidateResult(&function,candidateResultsMap,AllIfObjInCurFunc,MeaningDict);
    }
}


/**
 * @description: 处理Function内的候选对象
 * @param {Function} *function
 * @param {map<StringRef,vector<Instruction*>>} candidateResultsMap
 * @return {*}
 */
void handleCandidateResult(Function *function,map<string,vector<Instruction*>> candidateResultsMap,vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict){
    // errs()<<"candidate map size: "<<candidateResultsMap.size()<<"\n";
    // if (!function->getSubprogram()->getName().equals(StringRef("AE_3_AssociateConfirmationAccept")))
    // {
    //     return;
    // }
    
    for (auto onefunction : candidateResultsMap)
    { 
        // errs()<<"onefunction: "<<onefunction.first<<"\n";
        //如果语义是状态机变量，那么置flags，便于后续处理中分流、插桩
        bool isStateMachineArgsFlags = false;
        string targetName = onefunction.first;
        // errs()<<targetName<<"(targetname)\n";
        // if (checkArgsMeaningIfOK(MeaningDict,targetName,StateMachine_Args_Pos,1)) {
        // if (checkArgsMeaningIfOK(MeaningDict,targetName,New_StateMachine_Args_Pos,1)) {
        //     // isStateMachineArgsFlags = true;
        //     isStateMachineArgsFlags = true;
        //     // errs()<<"name: "<<targetName<<"\tisStateMachineArgsFlags: "<<isStateMachineArgsFlags<<"\n";
        // }

        for (auto inst : onefunction.second)
        {
            int error = 0;

            Function *function = inst->getFunction(); 
            SwitchInst *res_switchInst =NULL;
            //判断目标是否位于IF条件表达式中 -> 监控第一个 × 
            // errs()<<"[function] "<<function->getName()<<"\n";
            // errs()<< "inst: "<<*inst<<"\n";
            if (checkInstructionIfIF(inst,AllIfObjInCurFunc))
            {
                //获取inst属于哪一个IF Obj，便于在其if(true){...}内插桩
                int ifObjPos = findInstFromWhichIFObj(AllIfObjInCurFunc,inst);
                if (ifObjPos == -1)
                {
                    continue;
                }
                //获取当前ifobj
                IfConditionObj ifObjPtr = AllIfObjInCurFunc[ifObjPos];
                

                //IRBuilder初始化，位置:if true branch
                IRBuilder <>IRB(ifObjPtr.branchTrueBeginBB);
                //[+] 状态机变量分流
                //约束一下状态机变量范围:如果if {code} 中 code基本块个数少，就跳过，这样可以过滤一些（1.没有调用函数且基本块少）
                /*
                    对于单独的IF 状态机变量识别的方案（语义部分）
                        1.New_StateMachine字典，这个范围小，严格匹配语义的话，复杂度可以小一点
                        2.StateMacihne字典，这个范围大，即使严格匹配，复杂度的阈值要大一点
                */
                int singleIfResult_strict = checkSingleIfCodeComplexity(ifObjPtr,20);
                int singleIfResult_normal = checkSingleIfCodeComplexity(ifObjPtr,40);
                int sematic_strict = checkArgsMeaningIfOK(MeaningDict,targetName,New_StateMachine_Args_Pos,1);
                int sematic_normal = checkArgsMeaningIfOK(MeaningDict,targetName,StateMachine_Args_Pos,1);

                if ((checkIcmpInstFromTargetPoint(inst) && checkFuncInStateMachineCallGraph(function->getSubprogram()->getFilename())) && ((sematic_strict && singleIfResult_strict)||(sematic_normal && singleIfResult_normal)))
                // if (singleIfResult == 1 && isStateMachineArgsFlags && (checkIcmpInstFromTargetPoint(inst) || checkArgsMeaningIfOK(MeaningDict,targetName,StateMachine_Args_Pos,1)) && checkFuncInStateMachineCallGraph(function->getSubprogram()->getFilename()))
                {
                    Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curStateMachineNum,true);
                    int StateType = identifyStateVarType(determineWhitchToMonitor(inst));
                    if (StateType == -1){
                        continue;
                    }
                    ArrayRef<Value*> funcArgs = {determineWhitchToMonitor(inst),Constant_curStateMachineNum};
                    InsertFunction(function->getParent(),ifObjPtr.branchTrueBeginBB->getFirstNonPHI(),determineWhitchToMonitor(inst),StateMachineScene,StateType,funcArgs,error);
                    if (error == 0)
                    {
                        if (inst->getFunction()->getSubprogram()->getName().str() == "OSSL_PARAM_get_int32"){
                            errs()<<"sematic_strict: "<<sematic_strict<<"\t"<<sematic_normal<<"\n";
                            assert(1==2);
                        }

                        string filename = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str() + "***";
                        // addInstrumentInfoToVector(filename,inst->getDebugLoc()->getLine(),inst->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(inst->getOperand(0)->getName()).str(),StateMachineScene,StateType);
                        addInstrumentInfoToVector(filename,inst->getDebugLoc()->getLine(),inst->getFunction()->getSubprogram()->getName().str(),onefunction.first,StateMachineScene,StateType);
                    }
                }else{ 
                    //普通的状态变量
                    Constant *Constant_curSubStateNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curSubStateNum,true);// 构造函数参数
                    int StateType = identifyStateType(determineWhitchToMonitor(inst));
                    if (StateType == -1){
                        continue;
                    }
                    ArrayRef<Value*> funcArgs = {determineWhitchToMonitor(inst),Constant_curSubStateNum};
                    // errs()<<*inst<<"\n";
                    // errs()<<*ifObjPtr.branchTrueBeginBB<<"\n";
                    // errs()<<*ifObjPtr.branchTrueBeginBB->getFirstNonPHI()<<"\n";
                    // 插入具体函数
                    InsertFunction(function->getParent(),ifObjPtr.branchTrueBeginBB->getFirstNonPHI(),determineWhitchToMonitor(inst),SingleScene,StateType,funcArgs,error);
                    if (error==0){
                        AlreadyMonitorVarVec.push_back(determineWhitchToMonitor(inst));
                        //导出插桩日志
                        string filename = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
                        // addInstrumentInfoToVector(filename,inst->getDebugLoc()->getLine(),inst->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(inst->getOperand(0)->getName()).str(),SingleScene,StateType);
                        addInstrumentInfoToVector(filename,inst->getDebugLoc()->getLine(),inst->getFunction()->getSubprogram()->getName().str(),onefunction.first,SingleScene,StateType);
                    }
                }
                // break;
            }else if (checkInstructionIfSwitch(inst,&res_switchInst))   //判断目标是否位于Switch表达式中
            {
                // [+] 状态机变量分流
                if (isStateMachineArgsFlags && checkFuncInStateMachineCallGraph(function->getSubprogram()->getFilename())){
                    Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curStateMachineNum,true);
                    Instruction *swCondition = dyn_cast<Instruction>(res_switchInst->getCondition());
                    int StateType = identifyStateVarType(determineWhitchToMonitor(inst));
                    assert(StateType!=-1);
                    ArrayRef<Value*> funcArgs = {swCondition,Constant_curStateMachineNum};
                    for (auto &oneCase : res_switchInst->cases())
                    {
                        BasicBlock *caseBB = oneCase.getCaseSuccessor();
                        //插桩
                        InsertFunction(function->getParent(),caseBB->getFirstNonPHI(),swCondition,StateMachineScene,StateType,funcArgs,error);
                        if (error==0){
                            AlreadyMonitorVarVec.push_back(swCondition);
                            //导出插桩日志
                            string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
                            // addInstrumentInfoToVector(wholepath,swCondition->getDebugLoc()->getLine(),swCondition->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(swCondition->getOperand(0)->getName()),StateMachineScene,StateType);
                            addInstrumentInfoToVector(wholepath,swCondition->getDebugLoc()->getLine(),swCondition->getFunction()->getSubprogram()->getName().str(),onefunction.first,StateMachineScene,StateType);
                        }
                    }
                }else{
                    //构造函数参数
                    Constant *Constant_curSubStateNum= ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curSubStateNum,true);
                    Instruction *swCondition = dyn_cast<Instruction>(res_switchInst->getCondition());
                    int StateType = identifyStateType(swCondition);
                    if (StateType == -1){
                        continue;
                    }
                    ArrayRef<Value*> funcArgs = {swCondition,Constant_curSubStateNum};
                    //提取switch 各case，便于插桩
                    for (auto &oneCase : res_switchInst->cases())
                    {
                        BasicBlock *caseBB = oneCase.getCaseSuccessor();
                        //插桩
                        InsertFunction(function->getParent(),caseBB->getFirstNonPHI(),swCondition,SingleScene,StateType,funcArgs,error);
                        if (error==0){
                            AlreadyMonitorVarVec.push_back(swCondition);
                            //导出插桩日志
                            string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
                            // addInstrumentInfoToVector(wholepath,swCondition->getDebugLoc()->getLine(),swCondition->getFunction()->getSubprogram()->getName().str(),delLLVMIRInfoForVar(swCondition->getOperand(0)->getName()),SingleScene,StateType);
                            addInstrumentInfoToVector(wholepath,swCondition->getDebugLoc()->getLine(),swCondition->getFunction()->getSubprogram()->getName().str(),onefunction.first,SingleScene,StateType);
                        }
                    }
                }
            }
        }
    }
}
/**
 * @description: 检查指令是否来源于switch
 * @param {Instruction} *instruction
 * @param {Instruction} *res_switchInst 存放属于的那个switch inst
 * @return {*}
 */
bool checkInstructionIfSwitch(Instruction *instruction,SwitchInst **res_switchInst){
    BasicBlock *curBasicBlock = instruction->getParent();

    BasicBlock::iterator iter(instruction);
    for (; iter != curBasicBlock->end(); iter++)
    {
        Instruction *curInst = &*iter;
        if (isa<SwitchInst>(curInst))
        {
            break;
        }   
    }
    if (iter == curBasicBlock->end())
    {
        return false;
    }

    SwitchInst *switchInst = dyn_cast<SwitchInst>(&*iter);
    if (switchInst)
    {
        Instruction *conditionInst = dyn_cast<Instruction>(switchInst->getCondition());
        if (conditionInst)
        {
            if (checkInstructionIfIcmpCondition(curBasicBlock,instruction,conditionInst))
            {
                *res_switchInst = switchInst;
                return true;
            }
        }
    }
    return false;
}
/**
 * @description: 检查instruction是否来源于函数的某个参数(暂不支持结构体类型)
 * @param {Function} *function
 * @param {Instruction} *instruction
 * @param {int} &ArgPos
 * @return {*}
 */
bool checkIfInstFromArgs(Function *function,Instruction *instruction,int &ArgPos){
    // [1] check If Inst From Args
    // Function *function = instruction->getFunction();
    //通常这种instruction的名称以.addr结尾
    if (!instruction->getOperand(0)->getName().endswith(StringRef(".addr")) || instruction->getOperand(0)->getName().empty())
    {
        return false;
    }
    StringRef withoutAddrName = instruction->getOperand(0)->getName().substr(0,instruction->getOperand(0)->getName().size()-5);
    //遍历函数参数，获取需要处理的那个函数参数目标
    size_t i;
    for (i = 0; i < function->arg_size(); i++)
    {
        // errs()<<"checkIfInstFromArgs: "<<instruction->getOperand(0)->getName()<<"\t"<<withoutAddrName<<"\n";
        if (instruction->getOperand(0)->getName().equals_lower(withoutAddrName))
        {
            break;
        }
    }
    if (i == function->arg_size())
    {
        return false;
    }
    ArgPos = i;
    return true;
}
/**
 * @description: 检查参数args在function->begin() ~ endInst中是否以任意形式被使用过
 * @param {Function} *function
 * @param {Instruction} *endInst
 * @param {Argument} *args
 * @param {StoreInst} **finalStoreInst : 返回对函数参数赋值的store指令
 * @return {*}
 */
bool checkArgsOnlyUseOnce(Function *function,Instruction *endInst,Argument *args,StoreInst **finalStoreInst){
    errs()<<"checkArgsOnlyUseOnce: "<<*args<<"\tendInst: "<<*endInst<<"\n";
    //遍历function begin ~ endInst,匹配对args赋值的store指令
    inst_iterator iter = inst_begin(function);
    for (; &*iter != endInst;iter++)
    {
        StoreInst *storeInst = dyn_cast<StoreInst>(&*iter);
        if (storeInst && (storeInst->getOperand(0)->getType()==args->getType()))
        {
            break;
        }
    }
    StoreInst *storeInst = dyn_cast<StoreInst>(&*iter);
    if (storeInst && (storeInst->getOperand(0)->getType()==args->getType()))
    {
        // errs()<<"候选插桩监控: "<<*storeInst<<"\t[0]:"<<*storeInst->getOperand(0)<<"\t[1]:"<<*storeInst->getOperand(1)<<"\n";
        //遍历store Inst ~ endInst，检查有无对qos.addr操作的指令，如果有则插桩监控if中的 return false；没有则创建load指令并监控，return true
        for (++iter; &*iter != endInst;iter++)
        {
            Instruction *otherInst = &*iter;
            for (auto &op : otherInst->operands())
            {
                if (op.get() == storeInst->getOperand(1))
                {
                    //被使用了
                    return false;
                }
            }
        }
        // errs()<<"可以直接监控！\n";
        *finalStoreInst = storeInst;
        return true;
    }
    return false;
}
/*
案例:
    if.end7:                                          ; preds = %if.else5, %if.then3
    %5 = load i8, i8* %qos.addr, align 1, !dbg !67077
    %conv8 = zext i8 %5 to i32, !dbg !67077
    %cmp = icmp sgt i32 %conv8, 0, !dbg !67079
    br i1 %cmp, label %if.then10, label %if.end12, !dbg !67080

-> 识别得到的指令是%5,但icmp条件表达式中的指令是%cmp,换言之应当监控%cmp,而非%5
*/
Instruction* determineWhitchToMonitor(Instruction *instruction){
    BasicBlock *basicBlock = instruction->getParent();

    queue<Instruction*> fqueue;
    fqueue.push(instruction);

    while (fqueue.size() != 0)
    {
        Instruction *inst = fqueue.front();
        fqueue.pop();

        for (auto user : inst->users())
        {
            if (CallInst *callInst = dyn_cast<CallInst>(user))
            {
                continue;
            }
            if (ICmpInst *icmpInst = dyn_cast<ICmpInst>(user))
            {
                if (icmpInst->getParent()==basicBlock)
                {
                    return inst;
                }
            }else if (SwitchInst *swInst = dyn_cast<SwitchInst>(user))
            {
                if (swInst->getParent()==basicBlock)
                {
                    return inst;
                }
            }else{
                Instruction *user_inst = dyn_cast<Instruction>(user);
                if (user_inst && user_inst->getParent()==basicBlock)
                {
                    fqueue.push(user_inst);
                }
            }
        }
    }
    return instruction;
}
/**
 * @description: 从指定的指令开始向下寻找icmp指令，判断icmp是否为==/!=，且取值是否是状态机中的值
 * @param {Instruction} *targetPoint
 * @return {*}
 */
bool checkIcmpInstFromTargetPoint(Instruction *targetPoint){
    BasicBlock *basicblock = targetPoint->getParent();
    
    for (BasicBlock::iterator iter(targetPoint); iter != basicblock->end(); iter++)
    {
        Instruction *curInst = &*iter;
        if (isa<ICmpInst>(curInst))
        {
            ICmpInst *icmpInst = cast<ICmpInst>(curInst);
            // errs()<<"找到icmpInst "<<*icmpInst<<"\n";
            if ((icmpInst->getPredicate()== CmpInst::ICMP_EQ || icmpInst->getPredicate()== CmpInst::ICMP_NE) && icmpInst->getNumOperands() >= 2){
                if (stateMachineAssistInfo.SwCaseStateVal.size() == 0)  //如果Switch case大小为0，没有必要继续后续的比较，避免误判
                {
                    return true;
                }
                for (Use &operand : icmpInst->operands()){
                    Value *operandVal = cast<Value>(&operand);
                    if (ConstantInt *CI = dyn_cast<ConstantInt>(operandVal)){
                        int val = CI->getZExtValue();
                        //检查icmp value是否与switch中的值相同
                        if (find(stateMachineAssistInfo.SwCaseStateVal.begin(),stateMachineAssistInfo.SwCaseStateVal.end(),val) != stateMachineAssistInfo.SwCaseStateVal.end())
                        {
                            return true;
                        }
                    }
                }

                
                
            }
        }   
    }
    return false;
}