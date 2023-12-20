#include "switchmodule.h"
#include "globalvar.h"

using namespace std;
using namespace llvm;

extern int curStateMachineNum;
extern int curSubStateNum;

extern vector<Value*> AlreadyMonitorVarVec;

extern int PROGRAM_LANG_FLAGS;

extern Word2VecData word2vecdata;
extern bool IfEnableWord2vec;

extern vector<string> stateMachineCallGraph;


extern StateMachineVarAssistInfo stateMachineAssistInfo;

/**
 * @description: 提取程序中Switch类型关键变量
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,
 * @param vector<vector<string>> MeaningDict,
 * @param vector<Instruction*>} AlreadyMonitorVarVec
 * @return {*}
 */
void handleSwitch(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> &MeaningDict){

    vector<SwitchInstrumentObj> switchResults;  //保存组合类型的候选Switch对象
    vector<SwitchInstrumentObj> switchSingleResults;    //保存普通类型的Switch对象
    vector<SwitchInstrumentObj> switchStateMachineResults;  //保存状态机级别的候选Swtich对象

    for (auto &function : M.getFunctionList())
    {
        // if (!function.getName().equals(StringRef("dhcp6_no_relay")))
        // {
        //     continue;
        // }
        
        if (CallGraphMap.find(&function) == CallGraphMap.end() || function.isDeclaration() || function.getSubprogram()==NULL)
            continue;
        
        #ifdef DEBUG  
        errs()<<"[handleswitch] function name: "<<function.getSubprogram()->getName()<<"\n";
        #endif

        for (inst_iterator Inst = inst_begin(function),End = inst_end(function);Inst != End; ++Inst)
        {
            Instruction *instruction = cast<Instruction>(&*Inst);
            
            if (isa<SwitchInst>(instruction) && instruction->getDebugLoc().get() != NULL)
            {
                SwitchInst *swInst = cast<SwitchInst>(instruction);
                #ifdef DEBUG
                errs()<<"sw condition:\t"<<*swInst->getCondition()<<"\n";
                #endif

                if (!isa<Instruction>(swInst->getCondition()))
                {
                    continue;
                }

                // 1. 提取condition语义
                string semantics = extractSwitchSemantics(instruction);
                #ifdef DEBUG
                errs()<<"condition args: "<<semantics<<"\n";
                #endif
                // 2. 提取Switch各Case分支的函数调用和基本块情况
                vector<SwitchCase> swCase;
                int error = 0;
                extractAllCase(instruction,swCase,error);
                // 3. 检查各Case分支特征、复杂度、语义、函数名称
                if (checkCaseFeatures(swCase) && error==0)
                {
                    #ifdef DEBUG
                    // errs()<<"Final检测: "<<*swInst<<"\n";
                    #endif
                    // 4. 不管语义是否符合，都保存为候选目标，提供给动态分析调用函数来进一步筛选
                    extractInstInfoForMealyVar(instruction,semantics,0);
                    continue;

                    // 检查语义是否符合，if yes -> check semantics , if not -> 普通类型的变量监控
                    if (checkArgsMeaningIfOK(MeaningDict,semantics,Protocol_Args_Pos,0))
                    {
                        // errs()<<"semantics: "<<semantics<<"\t"<<function.getSubprogram()->getFilename()<<"\n";
                        // 增加一个状态字典检测，目的是筛选出状态机的状态变量；如果是，则按照新的监控方式插桩监控，否则不是，则照旧，现有监控；
                        if (checkArgsMeaningIfOK(MeaningDict,semantics,StateMachine_Args_Pos,0) && checkFuncInStateMachineCallGraph(function.getSubprogram()->getFilename()))
                        {  
                            #ifdef DEBUG
                            errs()<<"Switch StateMachine检查成功,保存为候选目标\n";
                            #endif
                            // errs()<<"自学习: "<<semantics<<"\tsize: "<<MeaningDict[StateMachine_Args_Pos].size()<<"\n";
                            MeaningDict[StateMachine_Args_Pos].push_back(semantics);    //自学习:保存这个词汇
                            MeaningDict[New_StateMachine_Args_Pos].push_back(semantics);
                            extractSwCaseValue(swInst,stateMachineAssistInfo.SwCaseStateVal);   //提取case val
                            SwitchInstrumentObj obj;
                            obj.swinst = swInst;
                            obj.swcondition = cast<Instruction>(swInst->getCondition());
                            obj.swCase = swCase;
                            obj.semantics = semantics;
                            stateMachineAssistInfo.VarType.push_back(identifyStateVarType(obj.swcondition));    //保存协议状态机变量的类型
                            switchStateMachineResults.push_back(obj);
                        }
                        else{
                            // #ifdef DEBUG
                            // errs()<<"Switch Multiscene检查成功,保存为候选目标\n";
                            // #endif
                            // SwitchInstrumentObj obj;
                            // obj.swinst = swInst;
                            // obj.swcondition = cast<Instruction>(swInst->getCondition());
                            // obj.swCase = swCase;
                            // obj.semantics = semantics;
                            // MeaningDict[Protocol_Args_Pos].push_back(semantics); //子状态自学习
                            // // 保存为候选目标，后续检查调用函数以进一步的筛选
                            // switchResults.push_back(obj); 
                        }
                    }else{
                        // // 以普通类型的监控方式监控
                        // // 在Switch的各Case上插桩，而非在switch前插桩监控
                        // SwitchInstrumentObj obj;
                        // obj.swinst = swInst;
                        // obj.swcondition = cast<Instruction>(swInst->getCondition());
                        // obj.swCase = swCase;
                        // obj.semantics = semantics;
                        // switchSingleResults.push_back(obj); 
                    }
                }
                // else{
                //     //如果分支个数满足3个，且switch条件语义与协议有关，则以普通变量变量监控
                //     if (swCase.size() > 3 && checkArgsMeaningIfOK(MeaningDict,semantics,Protocol_Args_Pos,0))
                //     {
                //         SwitchInstrumentObj obj;
                //         obj.swinst = swInst;
                //         obj.swcondition = cast<Instruction>(swInst->getCondition());
                //         obj.swCase = swCase;
                //         obj.semantics = semantics;
                //         switchSingleResults.push_back(obj); 
                //     }
                // }
            }
        }
    }
    handleCandidateStateMachineSwitch(switchStateMachineResults);

    // 处理候选的SwitchObj：如果switch类型且组合类型的有多个结果，则检查其函数名称的语义，符合的才监控
    handleCandidateSwitchObj(switchResults,MeaningDict);
    // 处理普通类型的SwitchObj，直接在各case上插桩
    handleCandidateSwitchObj_NotStateCondition(switchSingleResults);
    
}

/**
 * @description: 提取程序中IF代码块，并分别进行各自的处理
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict,vector<Instruction*>} AlreadyMonitorVarVec
 * @return {*}
 */
void handleIfElse(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict){

    for (auto &function : M.getFunctionList())
    {
        vector<IfConditionObj> AllIfObjInCurFunc;    //存储Function内所有的独立If
        if (CallGraphMap.find(&function) == CallGraphMap.end())
            continue;

        extractIFObjInCurFunction(&function,AllIfObjInCurFunc);
        
        if (AllIfObjInCurFunc.size()==0)
        {
            continue;
        }
        analysisAllIfObjInCurFunc(AllIfObjInCurFunc,MeaningDict);
    }
}
/**
 * @description: 分析当前函数中提取得到的If(){}对象，检查这些对象并打标记、对
 * @param {vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>>} MeaningDict
 * @return {*}
 */
void analysisAllIfObjInCurFunc(vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict){
    /*
        关注目标:
            1.strcmp、strncasecmp、自定义的比较函数
            2. ...
    */

   // 遍历所有的IF Obj
   int flag = 1;
   for (size_t i = 0; i < AllIfObjInCurFunc.size(); i++)
   {
        //检查其中一个IF Obj
        vector<IfTemplate> temVec;
        if (checkIsTargetIFObj(AllIfObjInCurFunc[i],temVec,MeaningDict) && AllIfObjInCurFunc[i].flags==0)
        {
            AllIfObjInCurFunc[i].flags = flag;
            // errs()<<"i ["<<i<<"] IFObj "<<AllIfObjInCurFunc[i].conditionBBVec[0]->getName()<<"\tflags: "<<flag<<"\n";
            // errs()<<"store flag: "<<flag<<" to AllIfObjInCurFunc["<<i<<"].flags\n";
            for (size_t j = i+1; j < AllIfObjInCurFunc.size(); j++)
            {
                if (checkIsTargetIFObjEqualTemplate(AllIfObjInCurFunc[j],temVec) && AllIfObjInCurFunc[j].flags==0){
                    AllIfObjInCurFunc[j].flags = flag;
                    // errs()<<"  j ["<<j<<"]\t"<<AllIfObjInCurFunc[j].conditionBBVec[0]->getName()<<"\tflags: "<<AllIfObjInCurFunc[j].flags<<"\n";
                    // errs()<<"Nice!\n";
                }
            }
            flag++;
        }
   }

   // 统计flags非0的个数，从高频到低频排序；相同flags的IF属于同一类型的
   map<int,int> CountFlagMap;   // 以<flags,次数>统计
   vector<pair<int,int>> CountFlagVec;
   queue<int> CandidateFlagQueue;
   for (auto ifConditionObj : AllIfObjInCurFunc)
   {
       if (ifConditionObj.flags==0)
       {
           continue;
       }
       CountFlagMap[ifConditionObj.flags]++;
       #ifdef DEBUG
       errs()<<ifConditionObj.conditionBBVec[0]->getName()<<" ~ "<<ifConditionObj.conditionBBVec[ifConditionObj.conditionBBVec.size()-1]->getName()<<" flags: "<<ifConditionObj.flags<<"\n";
       #endif
   }
   convertMaptoVec(CountFlagMap,CountFlagVec);
    #ifdef DEBUG
    // errs()<<"map size: "<<CountFlagMap.size()<<"\t"<<CountFlagVec.size()<<"\n";
    #endif
   if (CountFlagVec.size() != 0)
   {
        for (auto one : CountFlagVec)
        {
            //只关注至少有>3个分支的逻辑，如果>3，则记录其flags，后续再检测
            if (one.second > 3)
            {
                CandidateFlagQueue.push(one.first);
            }
        }
        //检查各分支的复杂度及函数调用情况，以决定是否监控
        handleCandidateIfObj(AllIfObjInCurFunc,CandidateFlagQueue,MeaningDict);
   }
   
}

/**
 * @description: 处理所有的候选IF，检查其中的特征，以判断是否具有我们关注的IF关键变量特征
 * @param {vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue,vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateIfObj(vector<IfConditionObj> AllIfObjInCurFunc,queue<int> CandidateFlagQueue,vector<vector<string>> MeaningDict){
    vector<int> candidatePrefixAndSuffixVec;    //保存具有前后缀特征的flags
    map<int,int> candidateMultiIFobjMap;     //以<flags,length>形式存放1.复杂且语义与协议有关2.有前后缀特征且与协议有关的,候选的组合型状态变量IF Obj
    map<int,int> candidateSingleIFObjMap;    //存放普通类型的IF Obj flags
    // 通过队列实现依次处理待处置的IF
    while (CandidateFlagQueue.size()!=0)
    {
        vector<IfConditionObj> candidateIFObjVec;

        int flags = CandidateFlagQueue.front();
        CandidateFlagQueue.pop();
        int wholeComplexity = 0;
        int length = 0;
        //遍历，开始处理
        for (size_t i = 0; i < AllIfObjInCurFunc.size(); i++)
        {
            if (AllIfObjInCurFunc[i].flags == flags){
                candidateIFObjVec.push_back(AllIfObjInCurFunc[i]);
                //计算复杂度
                length++;
                wholeComplexity += calIfComplexity(AllIfObjInCurFunc[i].branchTrueBeginBB,AllIfObjInCurFunc[i].branchTrueEndBB);
            }
        }
        wholeComplexity = (wholeComplexity/length); //IF的平均复杂度
        #ifdef DEBUG
        errs()<<"flag为: "<<flags<<" 的复杂度为: "<<(wholeComplexity)<<"\n";
        #endif
        //这个函数是遍历检查IF是否具有call相同前后缀特征的逻辑
        bool HasPreSuffFeatures = iterateIFCodeForCallFeature(AllIfObjInCurFunc,flags,length,candidatePrefixAndSuffixVec);
        #ifdef DEBUG
        errs()<<"有无前后缀特征: "<<HasPreSuffFeatures<<"\n";
        #endif
        // 无相同前后缀特征的逻辑
        if (HasPreSuffFeatures == false)
        {
            // 检查代码复杂度
            if (wholeComplexity>40)
            {
                // 检查函数参数的语义值，与协议有关 -> 暂存为组合类型变量；否则->普通变量监控
                if (checkIFObjArgsRelateProtocol(candidateIFObjVec,MeaningDict) || checkArgsMeaningIfOK(MeaningDict,AllIfObjInCurFunc[0].branchTrueBeginBB->getParent()->getName().str(),Call_Func_Pos,0))
                {
                    candidateMultiIFobjMap[flags] = length;
                }else{
                    candidateSingleIFObjMap[flags] = length;
                }   
            }
        }else{
            //有前后缀特征的逻辑
            //检查函数参数的语义值，与协议有关 -> 暂存为组合类型的变量；否则->普通变量
            string funcname = AllIfObjInCurFunc[0].branchTrueBeginBB->getParent()->getName().str();
            errs()<<"parent funcname "<<funcname<<"\n";
            if (checkIFObjArgsRelateProtocol(candidateIFObjVec,MeaningDict) || checkArgsMeaningIfOK(MeaningDict,AllIfObjInCurFunc[0].branchTrueBeginBB->getParent()->getName().str(),Call_Func_Pos,0))
            {
                #ifdef DEBUG
                errs()<<"当前被保存至:candidateMultiIFobjMap\n";
                #endif
                candidateMultiIFobjMap[flags] = length;
            }else{
                #ifdef DEBUG
                errs()<<"当前被保存至:candidateSingleIFObjMap\n";
                #endif
                candidateSingleIFObjMap[flags] = length;
            }
        }
    }
    
    //逐一处理组合类型的IF Obj
    vector<pair<int,int>> candidateMultiIFObjVec;
    convertMaptoVec(candidateMultiIFobjMap,candidateMultiIFObjVec);
    std::sort(candidateMultiIFObjVec.begin(),candidateMultiIFObjVec.end(),cmp_int_int);
    for (size_t i = 0; i < candidateMultiIFObjVec.size(); i++)
    {
        #ifdef DEBUG
        errs()<<"处理第"<<i+1<<"个候选组合类型的IF Obj\n";
        #endif
        handleMultiSingleIFObj(AllIfObjInCurFunc,MeaningDict,candidateMultiIFObjVec[i].first,1);
    }
    
    //逐一处理单一类型的IF Obj
    vector<pair<int,int>> candidateSingleIFObjVec;
    convertMaptoVec(candidateSingleIFObjMap,candidateSingleIFObjVec);
    std::sort(candidateSingleIFObjVec.begin(),candidateSingleIFObjVec.end(),cmp_int_int);
    for (size_t i = 0; i < candidateSingleIFObjVec.size(); i++)
    {
        #ifdef DEBUG
        errs()<<"处理第"<<i+1<<"个候选单一类型的IF Obj\n";
        #endif
        handleMultiSingleIFObj(AllIfObjInCurFunc,MeaningDict,candidateSingleIFObjVec[i].first,0);
    }
}
/**
 * @description: 处理候选的Multi和Single类型IF Obj：统计出现次数最多的函数参数、根据类型进行不同的监控
 * @param {vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>>} MeaningDict
 * @param {int} flags
 * @param {int} isMulti 1:Multi 0:Single
 * @return {*}
 */
void handleMultiSingleIFObj(vector<IfConditionObj> AllIfObjInCurFunc,vector<vector<string>> MeaningDict,int flags,int isMulti){  
    map<StringRef,int> CountArgsMap;    //统计函数参数出现的次数
    int length=0;
    //利用统计学，决定监控哪一个函数参数
    for (IfConditionObj obj : AllIfObjInCurFunc)
    {
        if (obj.flags == flags)
        {
            length++;
            for (BasicBlock *conditionBB : obj.conditionBBVec)
            {
                for (Instruction &instruction : *conditionBB)
                {
                    if (CallInst *callinst = dyn_cast<CallInst>(&instruction))
                    {
                        extractFuncArgsUsed(callinst,CountArgsMap);
                    }
                }
            }
        }
    }
    //即将监控的函数参数的名称
    vector<StringRef> CandidateArgsVec;

    vector<pair<StringRef,int>> CountArgsVec;
    convertMaptoVec(CountArgsMap,CountArgsVec);
    std::sort(CountArgsVec.begin(),CountArgsVec.end(),cmp);

    //如果处理的是组合类型的候选对象
    if (isMulti == 1)
    {
        for (auto one : CountArgsVec)
        {
            // errs()<<"Args name: "<<one.first.lower()<<"\t"<<one.second<<"\tlength: "<<length<<"\n";
            // if (one.second >= length*0.7)
            vector<string> stateDicVec = {"value"}; //字典，暂时不敢放进Protocol_Args_Pos中，只在此处使用，避免范围扩大
            if (one.second >= length*0.7 && (checkArgsMeaningIfOK(MeaningDict,one.first.lower(),Protocol_Args_Pos,0) || find(stateDicVec.begin(),stateDicVec.end(),one.first.lower()) != stateDicVec.end()))
            {
                // errs()<<"通过特征检查的: "<<one.first<<"\n";
                CandidateArgsVec.push_back(one.first);
            }
        }
        //已经确定要监控的参数位于：CandidateArgsVec中；若还有多个->监控最多的(第一个)，其他的普通类型监控

        //若candidateMultiIFObjVec中组合类型个数有多个，监控最长的，其他的作为普通类型[修改为所有都监控]
        for (size_t i = 0; i < CandidateArgsVec.size(); i++)
        {
            StringRef semantics = CandidateArgsVec[i];
            // if (i==0)
            // {
                //插桩监控:Multi类型
                insertFunctionForIF(AllIfObjInCurFunc,flags,MeaningDict,semantics,1);
            // }else{
            //     //插桩监控:Single类型
            //     insertFunctionForIF(AllIfObjInCurFunc,flags,semantics,0);
            // }
        }
    }//如果处理的是单一类型的候选对象
    else if (isMulti == 0)
    {
        for (auto one : CountArgsVec)
        {
            // errs()<<"Args2 name: "<<one.first<<"\t"<<one.second<<"\tlength: "<<length<<"\n";
            if (one.second >= length*0.7)
            {
                CandidateArgsVec.push_back(one.first);
            }
        }

        for (size_t i = 0; i < CandidateArgsVec.size(); i++)
        {
            StringRef semantics = CandidateArgsVec[i];

            //插桩监控:Single类型
            insertFunctionForIF(AllIfObjInCurFunc,flags,MeaningDict,semantics,isMulti);
        }

    }
    
}

void insertFunctionForIF(vector<IfConditionObj> AllIfObjInCurFunc,int flags,vector<vector<string>> MeanDict,StringRef semantics,int isMulti){
    bool ifInsert = false;  //标记是否插桩了
    for (IfConditionObj obj : AllIfObjInCurFunc)
    {
        if (obj.flags == flags)
        {
            //遍历IF Condition,检测并插桩
            if (insertFunctionInOneCondition(obj,MeanDict,semantics,isMulti))
            {
                ifInsert = true;
            }
        }
    }
    //插过桩，且是Multi类型插桩
    if (ifInsert && isMulti)
    {
        // curMultiStateNum++;
    }
    
}
bool insertFunctionInOneCondition(IfConditionObj obj,vector<vector<string>> MeanDict,StringRef semantics,int isMulti){
    int lastPos = obj.conditionBBVec.size()-1;
    BasicBlock *trueBranchBB = getTheTureBasicBlockofIF(obj.conditionBBVec[lastPos]);
    assert(trueBranchBB!=NULL);
    
    #ifdef DEBUG
    errs()<<"trueBranchBB: "<<trueBranchBB->getName()<<"\n";
    #endif
    
    for (BasicBlock *conditionBB : obj.conditionBBVec)
    {
        for (Instruction &instruction : *conditionBB)
        {
            if (CallInst *callinst = dyn_cast<CallInst>(&instruction))
            {
                Use *OL = callinst->getOperandList();
                for (int i = 0; i < callinst->getNumOperands()-1; i++)
                {
                    StringRef argsname;
                    Value *argVal = OL[i].get();
                    if (GEPOperator *gepOp = dyn_cast<GEPOperator>(argVal))
                    {
                        argsname = delLLVMIRInfoForVar(gepOp->getOperand(0)->getName());
                        // errs()<<"[重要]\n"<<isa<Instruction>(argVal)<<"\t"<<*argVal<<"\n";
                    }else if (Instruction *argInst = dyn_cast<Instruction>(argVal))
                    {
                        argsname = delLLVMIRInfoForVar(argVal->getName());
                        if (argsname.empty())
                        {
                            for (size_t i = 0; i < argInst->getNumOperands(); i++)
                            {
                                argsname = delLLVMIRInfoForVar(argInst->getOperand(i)->getName());
                                if (!argsname.empty())
                                {
                                    break;
                                }
                            }
                        }
                    }        
                    if (argsname.equals(semantics))
                    {
                        //监控当前的函数参数
                        //确定监控的位置: if true分支上
                        IRBuilder<> IRB(trueBranchBB);
                        //构造函数参数
                        Function *function = conditionBB->getParent();
                        string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();

                        Constant *Constant_curStateMachineNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curStateMachineNum,true);
                        Constant *Constant_curSubStateNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curSubStateNum,true);
                        int StateType = State_Char;
                        
                        Instruction *locateInst = trueBranchBB->getFirstNonPHIOrDbgOrLifetime();
                        
                        int error=0;
                        if (isMulti)
                        {
                            //StateMachine变量监控
                            vector<string> stateDicVec = {"value"};
                            if (checkFuncInStateMachineCallGraph(function->getSubprogram()->getFilename()))
                            {
                                if (checkArgsMeaningIfOK(MeanDict,semantics.lower(),StateMachine_Args_Pos,0) || find(stateDicVec.begin(),stateDicVec.end(),argsname) != stateDicVec.end()){
                                    MeanDict[StateMachine_Args_Pos].push_back(semantics.lower());   //自学习保存
                                    MeanDict[New_StateMachine_Args_Pos].push_back(semantics.lower());   //自学习保存
                                    // if(semantics.lower()=="p.data_type"){
                                    //     assert(1==2);
                                    // }
                                    ArrayRef<Value*> funcArgs = {argVal,Constant_curStateMachineNum};   //函数参数
                                    InsertFunction(function->getParent(),locateInst,argVal,StateMachineScene,Var_Type_Char,funcArgs,error);
                                    if (error==0){
                                        AlreadyMonitorVarVec.push_back(argVal);
                                        addInstrumentInfoToVector(wholepath,callinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),argsname,StateMachineScene,StateType);
                                        return true;
                                    }
                                }
                            }
                            //照旧监控
                            ArrayRef<Value*> funcArgs = {argVal,Constant_curSubStateNum};
                            InsertFunction(function->getParent(),locateInst,argVal,MultiScene,StateType,funcArgs,error);
                            if (error==0)
                            {
                                AlreadyMonitorVarVec.push_back(argVal);
                                addInstrumentInfoToVector(wholepath,callinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),argsname,MultiScene,StateType);
                                return true;
                            }
                        }else{
                            ArrayRef<Value*> funcArgs = {argVal,Constant_curSubStateNum};
                            InsertFunction(function->getParent(),locateInst,argVal,SingleScene,StateType,funcArgs,error);
                            if (error==0)
                            {
                                AlreadyMonitorVarVec.push_back(argVal);
                                addInstrumentInfoToVector(wholepath,callinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),argsname,SingleScene,StateType);
                                return true;
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
 * @description: 获取IF表达式为true时跳转的基本块
 * @param {BasicBlock} *basicblock
 * @return {*}
 */
BasicBlock* getTheTureBasicBlockofIF(BasicBlock *basicblock){
    for (BasicBlock *bb : successors(basicblock))
    {
        if (bb->getName().contains(StringRef("true")) || bb->getName().contains(StringRef("then")))
        {
            return bb;
        }
    }
    return NULL;
}
/**
 * @description: 检查函数的各参数语义，是否与协议有关
 * @param {vector<IfConditionObj> candidateIFObjVec,vector<vector<string>>} MeaningDict
 * @return {*}
 */
bool checkIFObjArgsRelateProtocol(vector<IfConditionObj> candidateIFObjVec,vector<vector<string>> MeaningDict){
    for (IfConditionObj obj : candidateIFObjVec)
    {
        for (BasicBlock *conditionBB : obj.conditionBBVec)
        {
            for (Instruction &instruction : *conditionBB)
            {
                if (CallInst *callinst = dyn_cast<CallInst>(&instruction))
                {
                    // vector<int> ArgVec;
                    if (checkFuncArgsIfRelateProtocol(callinst,MeaningDict))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
/**
 * @description: 检查IF(){...}中函数调用特征，是否具有相同的前后缀特征
 * @param {vector<IfConditionObj> AllIfObjInCurFunc,int flags,int length,vector<int>} &candidateResVec
 * @return {*}
 */
bool iterateIFCodeForCallFeature(vector<IfConditionObj> AllIfObjInCurFunc,int flags,int length,vector<int> &candidateResVec){

    map<StringRef,int> prefixCallFuncMap,suffixCallFuncMap;
    map<StringRef,int> functionCountMap; //辅助：避免出现每个case调用同一个函数的情况

    // int size = 0;

    for (size_t i = 0; i < AllIfObjInCurFunc.size(); i++)
    {

        if (AllIfObjInCurFunc[i].flags == flags){
            map<StringRef,int> prefixTmpCountMap,suffixTmpCountMap;  //如果一case中同时出现handleA和handleB，在本轮中map的计数只取1，换言之，对整个switch的统计而言，处理每个case分支时，其调用次数需是唯一的
            map<StringRef,int> functionTmpCountMap;
            //检查各case的调用函数特征
            BasicBlock *beginBB = AllIfObjInCurFunc[i].branchTrueBeginBB,*endBB = AllIfObjInCurFunc[i].branchTrueEndBB;

            for (Function::iterator beginIterator(beginBB),endIterator(endBB);beginIterator!=endIterator;beginIterator++){
                BasicBlock *basicblock = &*beginIterator;
                for (Instruction &instruction : *basicblock)
                {
                    if (CallInst *callInst = dyn_cast<CallInst>(&instruction))
                    {
                        if (callInst->getCalledFunction() && callInst->getCalledFunction()->isDeclaration()==0)
                        {
                            Function *function = callInst->getCalledFunction();
                            StringRef funcname;
                            if (PROGRAM_LANG_FLAGS)
                            {
                                funcname = function->getSubprogram()->getName();
                            }else{
                                funcname = function->getName();
                            }
                            #ifdef DEBUG
                            errs()<<"调用的函数:"<<funcname<<"\n";
                            #endif

                            // store prefix info
                            if (prefixTmpCountMap.find(funcname.substr(0,2)) == prefixTmpCountMap.end())
                            {
                                prefixTmpCountMap[funcname.substr(0,2)] = 1;
                                prefixCallFuncMap[funcname.substr(0,2)]++;
                            }
                            // store suffix info
                            if (suffixTmpCountMap.find(funcname.substr(funcname.size()-2)) == suffixTmpCountMap.end())
                            {
                                suffixTmpCountMap[funcname.substr(funcname.size()-2)] = 1;
                                suffixCallFuncMap[funcname.substr(funcname.size()-2)]++;
                            }
                            if (functionTmpCountMap.find(funcname) == functionTmpCountMap.end())
                            {
                                functionTmpCountMap[funcname] = 1;
                                functionCountMap[funcname]++;
                            }
                        }else if (callInst->getCalledFunction() == NULL){
                            // errs()<<"间接函数调用"<<*callInst<<"\t"<<callInst->getDebugLoc()->getDirectory()<<"/"<<callInst->getDebugLoc()->getFilename()<<"\tcode line: "<<callInst->getDebugLoc()->getLine()<<"\n";
                            string filename = callInst->getDebugLoc()->getDirectory().str() + "/" + callInst->getDebugLoc()->getFilename().str();
                            int codeline = callInst->getDebugLoc()->getLine();
                            int colnum = callInst->getDebugLoc()->getColumn();
                            string funcname = readContentFromTheFile(filename,codeline);
                            #ifdef DEBUG
                            errs()<<"读取到的内容是:"<<funcname<<"\n";
                            #endif
                            if (StartWith(funcname,string("Error")))
                            {
                                continue;
                            }else{
                                funcname = funcname.substr(colnum-1,-1);
                                int endPos = funcname.find_first_of("(");
                                funcname = funcname.substr(0,endPos);
                                // errs()<<"间接调用:"<<funcname<<"\n";
                            }
                            // size += funcname.size();
                            // store prefix info
                            //FIXME:考虑相对动态的取这个前缀，如取平均值再取一定比例，而不是限制死为2
                            // errs()<<"处理的函数:"<<funcname<<"\n";

                            if (funcname.size()<4)
                            {
                                continue;
                            }
                            

                            if (prefixTmpCountMap.find(funcname.substr(0,2)) == prefixTmpCountMap.end())
                            {
                                prefixTmpCountMap[funcname.substr(0,2)] = 1;
                                prefixCallFuncMap[funcname.substr(0,2)]++;
                            }
                            // store suffix info
                            if (suffixTmpCountMap.find(funcname.substr(funcname.size()-2)) == suffixTmpCountMap.end())
                            {
                                suffixTmpCountMap[funcname.substr(funcname.size()-2)] = 1;
                                suffixCallFuncMap[funcname.substr(funcname.size()-2)]++;
                            }
                            if (functionTmpCountMap.find(funcname) == functionTmpCountMap.end())
                            {
                                functionTmpCountMap[funcname] = 1;
                                functionCountMap[funcname]++;
                            }
                        }
                    }
                }
            }
        }
    }

    // errs()<<"平均长度是:"<<size/14<<"\n";

    if (checkSwitchCaseCallTimes(length,prefixCallFuncMap,functionCountMap) || checkSwitchCaseCallTimes(length,suffixCallFuncMap,functionCountMap))
    {
        // errs()<<"[*] 调用特征符合!\n";
        candidateResVec.push_back(flags);
        return true;
        // return;
    }
    // return;
    return false;
}
/**
 * @description: 计算if code的复杂度
 * @param {BasicBlock} *beginBB
 * @param {BasicBlock} *endBB
 * @return {*}
 */
int calIfComplexity(BasicBlock *beginBB,BasicBlock *endBB){
    #ifdef DEBUG
    errs()<<"calIfComplexity\t"<<beginBB->getName()<<" ~ "<<endBB->getName()<<"\n";
    #endif
    int complexity = 0;

    for (Function::iterator beginIterator(beginBB),endIterator(endBB);beginIterator!=endIterator;beginIterator++){
        complexity++;
    }

    // errs()<<"calIfComplexity return "<<complexity<<"\n";
    return complexity;
}

template<typename type1,typename type2>
void convertMaptoVec(map<type1,type2> countMap,vector<pair<type1,type2>> &vec){
    for (auto one:countMap)
    {
        pair<type1,type2> onepair;
        onepair.first = one.first;
        onepair.second = one.second;
        vec.push_back(onepair);
    }
    // std::sort(vec.begin(),vec.end(),cmp2);
}
/**
 * @description: 检查当前的IF Obj是否与模板信息一致
 * @param {IfConditionObj} ifConditionObj
 * @param {vector<IfTemplate>} temVec
 * @return {*}
 */
bool checkIsTargetIFObjEqualTemplate(IfConditionObj ifConditionObj,vector<IfTemplate> temVec){
    for (auto expressionBB : ifConditionObj.conditionBBVec)
    {
        for (Instruction &instruction : *expressionBB)
        {
            if (isa<ICmpInst>(&instruction) && isa<CallInst>(instruction.getOperand(0)))
            {
                //比较函数名称、参数名称
                CallInst *callinst = cast<CallInst>(instruction.getOperand(0));
                if (callinst->getCalledFunction())
                {
                    for (IfTemplate temp : temVec)
                    {
                        // errs()<<temp.function->getName()<<"\tvs\t"<<callinst->getCalledFunction()->getName()<<"\n";
                        if (temp.function == callinst->getCalledFunction())
                        {
                            //比较参数名称
                            Value *args = getArgsFromStrCmpFunction(callinst);
                            // errs()<<delLLVMIRInfoForVar(args->getName())<<"\tvs\t"<<temp.Args[0].operandName<<"\n";
                            if (args != NULL && (args == temp.Args[0].argval || delLLVMIRInfoForVar(args->getName()).equals(temp.Args[0].operandName)))
                            {
                                return true;
                            }
                            
                        }
                    }
                }
            }
        }
    }
    return false;
}
//FIXME:担心可能有问题/局限，待进一步剖析
bool checkIfStrCmpFunction(CallInst *callinst,Function *calledFunction){
    //库函数中的比较函数
    vector<StringRef> LibFunctionVec = {StringRef("strcmp"),StringRef("strncasecmp"),StringRef("strcasecmp")};
    if (find(LibFunctionVec.begin(),LibFunctionVec.end(),calledFunction->getName()) != LibFunctionVec.end())
    {
        return true;
    }else{
        // 检测是否可能是用户自定义的比较函数，可通过函数调用的特征识别->函数参数有一个globalvar，i8*
        // 若函数参数2~3个，且其中有一个global var，还有一个i8*
        // errs()<<"debug info : "<<*callinst<<"\t"<<callinst->getNumOperands()<<"\n";
        if (callinst->getNumOperands() == 3 || callinst->getNumOperands() == 4)
        {
            bool globalvarflags = false,stringflags = false;
            Use *OL = callinst->getOperandList();
            for (size_t i = 0; i < callinst->getNumOperands()-1; i++)
            {
                Value *argVal = OL[i].get();
                // errs()<<"i: "<<i<<" "<<*argVal<<"\t"<<isa<Instruction>(argVal)<<"\n";
                if (isa<GEPOperator>(argVal) && !isa<Instruction>(argVal))
                {
                    globalvarflags = true;
                }else if (Instruction *argInst = dyn_cast<Instruction>(argVal))
                {
                    if (argInst->getType()->isPointerTy() && argInst->getType()->getPointerElementType()->isIntegerTy(8)){
                        stringflags = true;
                    }   
                }else{
                    // errs()<<"都不是\n";
                }

            }
            if (globalvarflags && stringflags)
            {
                // errs()<<"[疑似用户自定义比较函数] "<<calledFunction->getName()<<"\n";
                return true;
            }
            
        }
    }
    return false;
}

Value* getArgsFromStrCmpFunction(CallInst *callInst){
    Use *OL = callInst->getOperandList();
    for (size_t i = 0; i < callInst->getNumOperands()-1; i++)
    {
        Value* argval = OL[i].get();
        if (checkArgsIfConstr(argval)){
            continue;
        }
        if (argval->getType()->isPointerTy())
        {
            PointerType *withPointerType = dyn_cast<PointerType>(argval->getType());
            if (withPointerType->getElementType()->isIntegerTy(8)){
                return argval;
            }
        }
    }
    return NULL;
}

/**
 * @description: 检查Function函数参数是否是常量字符串，便于确定监控监控的变量
 * @param {Value} *args
 * @return {*}
 */
bool checkArgsIfConstr(Value *args){
//   errs()<<"待检查的参数是"<<*args<<"\n";
  ConstantExpr *ce = dyn_cast<ConstantExpr>(args);
  if (ce)
  {
    if (ce->getOpcode() == Instruction::GetElementPtr)
    {
      if (GlobalVariable *annoteStr = dyn_cast<GlobalVariable>(ce->getOperand(0)))
      {
        if (annoteStr->hasInitializer()==0)
        {
          return false;
        }
        if (ConstantDataSequential *data = dyn_cast<ConstantDataSequential>(annoteStr->getInitializer()))
        {
          if (data->isString())
          {
            // errs()<<"Found data "<<data->getAsString()<<'\n';
            return true;
          }
        }
      }
    }
  }
  return false;
}
/**
 * @description: 检查当前IF Obj是否是我们的目标，是则制作模板，方便后续遍历检测同类型的
 * @param {IfConditionObj} ifConditionObj
 * @param {vector<IfTemplate> &temVec,vector<vector<string>>} MeaningDict
 * @return {*}
 */
bool checkIsTargetIFObj(IfConditionObj ifConditionObj,vector<IfTemplate> &temVec,vector<vector<string>> MeaningDict){
    //检查条件表达式,"打标记"
    for (auto expressionBB : ifConditionObj.conditionBBVec)
    {
        //找到是否有call指令且该指令是作为条件的
        for (Instruction &instruction : *expressionBB)
        {
            if (isa<ICmpInst>(&instruction) && isa<CallInst>(instruction.getOperand(0)))
            {
                CallInst *callinst = cast<CallInst>(instruction.getOperand(0));
                if (callinst->getCalledFunction())
                {
                    Function *calledFunction = callinst->getCalledFunction();
                    map<StringRef,int> CountArgsMap;
                    // errs()<<"新插入的函数获取参数语义: \n";
                    // extractFuncArgsUsed(callinst,CountArgsMap);
                    if (checkIfStrCmpFunction(callinst,calledFunction))
                    {
                        //制作一个模板，用于后续检测使用，只在First时制作
                        IfTemplate tem;
                        tem.function = calledFunction;
                        tem.icmpInst = cast<ICmpInst>(&instruction);

                        ArgStruct argstruct;
                        Value *args = getArgsFromStrCmpFunction(callinst);
                        if (args==NULL)
                        {
                            errs()<<"异常情况,跳过\n";
                            continue;
                        }
                        
                        argstruct.argval = args;
                        argstruct.argtype = args->getType();
                        if (!args->getName().empty())
                        {
                            argstruct.operandName = delLLVMIRInfoForVar(args->getName());
                        }else{
                            argstruct.operandName = "";  
                        }

                        tem.Args.push_back(argstruct);
                        
                        temVec.push_back(tem);
                    }
                    
                }
            }
        }
    }
    if (temVec.size()!=0)
    {
        return true;
    }
    return false;
}
/**
 * @description: 检查IF条件中调用的函数参数，是否与函数语义有关，有关则将
 * @param {CallInst} *callInst
 * @param {vector<vector<string>>} MeaningDict
 * @return {*}
 */
bool checkFuncArgsIfRelateProtocol(CallInst *callInst,vector<vector<string>> MeaningDict){
    Use *OL = callInst->getOperandList();
    for (int i = 0; i < callInst->getNumOperands()-1; i++)
    {
        string semantics;
        Value *argVal = OL[i].get();
        semantics = extractFunctionArgSemantics(argVal).lower();

        if (checkArgsMeaningIfOK(MeaningDict,semantics,Protocol_Args_Pos,0))
        {
            #ifdef DEBUG
            errs()<<"语义OK的嘛: "<<semantics<<"\n";
            #endif
            return true;
        }
    }
    return false;
}
void extractFuncArgsUsed(CallInst *callInst,map<StringRef,int> &CountArgsMap){
    Use *OL = callInst->getOperandList();
    for (int i = 0; i < callInst->getNumOperands()-1; i++)
    {
        StringRef semantics;
        Value *argVal = OL[i].get();
        //SXP:该函数目的是为了解决确定strcmp()中监控哪个函数的问题，memcached中出现，将length监控了，所以增加判断，只关注string类型
        if (!(argVal->getType()->isPointerTy() && argVal->getType()->getPointerElementType()->isIntegerTy(8))){
            continue;
        }
        semantics = extractFunctionArgSemantics(argVal);
        if (semantics.startswith(StringRef(".")))
        {
            continue;
        }
        
        // errs()<<"新插入的函数获取参数语义: "<<extractFunctionArgSemantics(argVal)<<"\t是不是指针类型"<<argVal->getType()->isPointerTy()<<"\n"; //if (argInst->getType()->isPointerTy() && argInst->getType()->getPointerElementType()->isIntegerTy(8)){
        // if (Instruction *argInst = dyn_cast<Instruction>(argVal))
        // {
        //     semantics = argInst->getName();
        //     if (semantics.empty())
        //     {
        //         for (size_t i = 0; i < argInst->getNumOperands(); i++)
        //         {
        //             semantics = delLLVMIRInfoForVar(argInst->getOperand(0)->getName());
        //             if (!semantics.empty())
        //             {
        //                 break;
        //             }
        //         }
        //     }
        // }
        // if (GEPOperator *gepOp = dyn_cast<GEPOperator>(argVal))
        // {
        //     semantics = delLLVMIRInfoForVar(gepOp->getOperand(0)->getName());
        // }
        // errs()<<"获取的函数参数语义值是 "<<semantics<<"\n";
        
        CountArgsMap[semantics]++;
    }

}

void storeTheIfObj(vector<IfConditionObj> &AllIfObjInCurFunc,vector<BasicBlock*> resultVec,vector<BasicBlock*> rangeBB,int IfObjNumber){
    IfConditionObj ifConditionObj;
    ifConditionObj.conditionBBVec = resultVec;
    ifConditionObj.flags = 0;
    ifConditionObj.branchTrueBeginBB = rangeBB[0];
    ifConditionObj.branchTrueEndBB = rangeBB[1];
    ifConditionObj.IfObjNumber = IfObjNumber;
    // errs()<<"ifConditionObj.IfObjNumber: "<<ifConditionObj.IfObjNumber<<"\n";
    AllIfObjInCurFunc.push_back(ifConditionObj);
}

/**
 * @description: 给定基本块curBB，找这个块的true后继为if.then开头的，放[0]，另外一个放[1]
 * @param {BasicBlock} *curBB
 * @param {vector<BasicBlock*>} &rangeBB
 * @return {*}
 */
void extractBranchTrue(BasicBlock *curBB,vector<BasicBlock*> &rangeBB){
    // if.then 开头的是beginBB,其他的是endBB;
    for (BasicBlock *bb : successors(curBB))
    {
        if (bb->getName().startswith(StringRef("if.then")))
        {
            rangeBB[0] = bb;
        }else{
            rangeBB[1] = bb;
        }
        
    }
    
}
/**
 * @description: 在有一个后继非if开头的基本块情况下，需进一步向后遍历提取相关块，使构成完成的IF表达式所需基本块
 * @param {BasicBlock} *basicblock
 * @param {vector<BasicBlock*>} &resultVec  相关基本块被保存的位置
 * @return {*}
 */
//[Done]遇到后面两个分支都不是if的时候，就退出去了，需要修复为：选择其中一个继续遍历（当然必须保证第一个if的两分支是if）
void traverseForIFCondition(BasicBlock *basicblock,vector<BasicBlock*> &resultVec){
    map<BasicBlock*,int> resultMap;

    if (resultMap.find(basicblock) == resultMap.end())
    {
        resultMap.insert(pair<BasicBlock*,int>(basicblock,1));
        resultVec.push_back(basicblock);
    }
    
    BasicBlock *succBB = getNotStartWithIfBasicBlock(basicblock);
    queue<BasicBlock*> bqueue;
    bqueue.push(succBB);
    while (bqueue.size()!=0)
    {
        BasicBlock *tmp = bqueue.front();
        bqueue.pop();
        // errs()<<"tmp "<<tmp->getName()<<'\n';
        if (checkSuccStartWithIfNumber(tmp)==2)
        {
            // errs()<<"IF Condition 终止块: "<<tmp->getName()<<"\n======\n";
            if (resultMap.find(tmp) == resultMap.end())
            {
                resultMap.insert(pair<BasicBlock*,int>(tmp,1));
                resultVec.push_back(tmp);
            }
            return;
        }else if (checkSuccStartWithIfNumber(tmp)==1)
        {
            tmp = getNotStartWithIfBasicBlock(tmp);
            // errs()<<"IF Condition 其他的块: "<<tmp->getName()<<"\n";
            bqueue.push(tmp);
            if (resultMap.find(tmp) == resultMap.end())
            {
                resultMap.insert(pair<BasicBlock*,int>(tmp,1));
                resultVec.push_back(tmp);
            }
        }else{
            // errs()<<"没找到IF条件终止？？？\n";
            // 应该选择一个，继续向下处理
            //不能选择while，将导致循环
            tmp = getNotStartWithIfBasicBlock(tmp);
            bqueue.push(tmp);
            if (resultMap.find(tmp) == resultMap.end())
            {
                resultMap.insert(pair<BasicBlock*,int>(tmp,1));
                resultVec.push_back(tmp);
            }

            // for (auto one : successors(tmp))
            // {
            //     if (resultMap.find(tmp) == resultMap.end())
            //     {
            //         resultMap.insert(pair<BasicBlock*,int>(tmp,1));
            //         resultVec.push_back(tmp);
            //         bqueue.push(tmp);
            //     }
            // }
        }
    }
    
}

BasicBlock* getNotStartWithIfBasicBlock(BasicBlock *basicblock){
    // errs()<<"basicblock "<<basicblock->getName()<<'\n';
    for (auto one : successors(basicblock))
    {
        // errs()<<"successors: "<<one->getName()<<"\n";
        // if has one successor is not start with if,then set flag
        if (!one->getName().startswith(StringRef("if.")) && !(basicblock->getName().equals(one->getName())))    //条件2:避免重复while.body，造成死循环
        {
            // errs()<<"选择 successors: "<<one->getName()<<"\n";
            return one;
        }
    }
    assert(2==3);
    return NULL;
}

int checkSuccStartWithIfNumber(BasicBlock *basicblock){
    int CountIF = 0;
    // iterate successors of the basicblock
    for (auto one : successors(basicblock))
    {
        // if has one successor is not start with if,then set flag
        if (one->getName().startswith(StringRef("if.")))
        {
            CountIF++;
        }
    }
    return CountIF;
}

bool checkIsIF(BasicBlock *basicblock){
    if (succ_size(basicblock)==2)
    {
        //均不以if开头
        if (checkSuccStartWithIfNumber(basicblock)==0)
        {
            return false;
        }
        //其中1个是if
        return true;
    }
    return false;
}

/**
 * @description: 处理候选的Switch目标
 * @param {vector<SwitchInstrumentObj> switchResults,
 * @param vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateSwitchObj(vector<SwitchInstrumentObj> switchResults,vector<vector<string>> MeaningDict){
    for (auto oneObj : switchResults)
    {
        Function *function = oneObj.swinst->getFunction();
        StringRef funcname;
        if (PROGRAM_LANG_FLAGS)
        {
            funcname = function->getSubprogram()->getName();
        }else{
            funcname = function->getName();
        }
        #ifdef DEBUG
        errs()<<"当前候选对象的函数名称:"<<funcname<<"\n";
        #endif
        //检查函数的语义信息，符合才...
        //FIXME:取消函数名称是否与协议有关的机制
        // if (!checkArgsMeaningIfOK(MeaningDict,funcname.lower(),Handle_Func_Pos))
        // {
        //     continue;
        // }

        // IRBuilder <>IRB(oneObj.swinst->getParent());
        //构造函数参数
        // Value *GlobalString_filename = IRB.CreateGlobalStringPtr(function->getSubprogram()->getFilename());
        // Constant *Constant_codeline = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),function->getSubprogram()->getLine(),true);
        // Constant *Constant_curMultiStateNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curMultiStateNum,true);
        Constant *Constant_curSubStateNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curSubStateNum,true);
        int StateType = identifyStateType(oneObj.swcondition);
        if (StateType == -1){
            continue;
        }
        // int switchNum = 1;
        int error = 0;
        for (auto swCase : oneObj.swCase)
        {
            #ifdef DEBUG
            errs()<<swCase.beginBB->getName()<<"\n";
            #endif
            Instruction *firstInst = swCase.beginBB->getFirstNonPHI();
            ArrayRef<Value*> funcArgs = {oneObj.swcondition,Constant_curSubStateNum};
            //于Case首条指令处插入具体插桩函数
            InsertFunction(function->getParent(),firstInst,oneObj.swcondition,MultiScene,StateType,funcArgs,error);
            if (error == 0){
                AlreadyMonitorVarVec.push_back(oneObj.swcondition);
            }
        }
        if (error == 0){
            // 记录日志
            string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
            if (PROGRAM_LANG_FLAGS)
            {
                addInstrumentInfoToVector(wholepath,oneObj.swinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),delLLVMIRInfoForVar(oneObj.semantics),MultiScene,StateType);
            }else{
                addInstrumentInfoToVector(wholepath,oneObj.swinst->getDebugLoc()->getLine(),function->getName(),delLLVMIRInfoForVar(oneObj.semantics),MultiScene,StateType);
            }
        }
    }
}
/**
 * @description: 处理候选的Switch目标（状态机变量）
 * @param {vector<SwitchInstrumentObj> switchResults,
 * @param vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateStateMachineSwitch(vector<SwitchInstrumentObj> switchResults){
    for (auto oneObj : switchResults)
    {
        Function *function = oneObj.swinst->getFunction();
        StringRef funcname = function->getSubprogram()->getName();
        Constant *curStateMachineNumLLVM = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curStateMachineNum,true);
        int Var_Type = identifyStateVarType(oneObj.swcondition);
        assert(Var_Type!=-1);
        //遍历case 插桩
        int error = 0;
        for (auto swCase : oneObj.swCase)
        {
            Instruction *firstInst = swCase.beginBB->getFirstNonPHI();
            ArrayRef<Value*> funcArgs = {oneObj.swcondition,curStateMachineNumLLVM};
            //插桩
            InsertFunction(function->getParent(),firstInst,oneObj.swcondition,StateMachineScene,Var_Type,funcArgs,error);
            if (error == 0){
                AlreadyMonitorVarVec.push_back(oneObj.swcondition);
            }
        }
        if (error == 0){
            curStateMachineNum++;
            // 记录日志
            string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
            addInstrumentInfoToVector(wholepath,oneObj.swinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),delLLVMIRInfoForVar(oneObj.semantics),StateMachineScene,Var_Type);
        }   
    }
}

/**
 * @description: 处理候选的Switch目标（条件表达式与协议状态无关）
 * @param {vector<SwitchInstrumentObj> switchResults,
 * @param vector<vector<string>>} MeaningDict
 * @return {*}
 */
void handleCandidateSwitchObj_NotStateCondition(vector<SwitchInstrumentObj> switchResults){
    for (auto oneObj : switchResults)
    {
        Function *function = oneObj.swinst->getFunction();
        StringRef funcname;
        if (PROGRAM_LANG_FLAGS)
        {
            funcname = function->getSubprogram()->getName();
        }else{
            funcname = function->getName();
        }
        // errs()<<"当前候选对象的函数名称:"<<funcname<<"\n";

        // IRBuilder <>IRB(oneObj.swinst->getParent());
        //构造函数参数
        Constant *Constant_curSubStateNum = ConstantInt::get(IntegerType::getInt32Ty(function->getContext()),curSubStateNum,true);
        int StateType = identifyStateType(oneObj.swcondition);
        if (StateType == -1){
            continue;
        }

        int error = 0;
        for (auto swCase : oneObj.swCase)
        {
            // errs()<<swCase.beginBB->getName()<<"\n";
            Instruction *firstInst = swCase.beginBB->getFirstNonPHI();
            ArrayRef<Value*> funcArgs = {oneObj.swcondition,Constant_curSubStateNum};
            //于Case首条指令处插入具体插桩函数
            InsertFunction(function->getParent(),firstInst,oneObj.swcondition,SingleScene,StateType,funcArgs,error);
            if (error == 0){
                AlreadyMonitorVarVec.push_back(oneObj.swcondition);
            }
        }
        if (error == 0){
            // 记录日志
            string wholepath = function->getSubprogram()->getDirectory().str() + "/" + function->getSubprogram()->getFilename().str();
            if (PROGRAM_LANG_FLAGS)
            {
                addInstrumentInfoToVector(wholepath,oneObj.swinst->getDebugLoc()->getLine(),function->getSubprogram()->getName(),delLLVMIRInfoForVar(oneObj.semantics),SingleScene,StateType);
            }else{
                addInstrumentInfoToVector(wholepath,oneObj.swinst->getDebugLoc()->getLine(),function->getName(),delLLVMIRInfoForVar(oneObj.semantics),SingleScene,StateType);
            }
        }
    }
}


/**
 * @description: 统计Switch代码的整体复杂度
 * @param {vector<SwitchCase>} swCase
 * @return {*}
 */
int countSwCodeComplexity(vector<SwitchCase> swCase){
    int averageBasicBlockNum = 0;
    for (auto &onecase : swCase)
    {
        #ifdef DEBUG
        errs()<<"[onecase]"<<onecase.beginBB->getName()<<"~"<<onecase.endBB->getName()<<"\t复杂度:"<<onecase.BBnum<<"\n";
        #endif
        averageBasicBlockNum += onecase.BBnum;
    }
    averageBasicBlockNum = averageBasicBlockNum/(swCase.size());
    #ifdef DEBUG
    errs()<<"平均复杂度:"<<averageBasicBlockNum<<"\tSwitch size:"<<swCase.size()<<"\n";
    #endif
    return averageBasicBlockNum;
}
/**
 * @description: 检查Switch各Case的特征，如复杂度、调用函数名称的前后缀
 * @param {vector<SwitchCase>} swCase
 * @return {*}
 */
bool checkCaseFeatures(vector<SwitchCase> swCase){
    if (swCase.size() < 3)
    {
        return false;
    }
    
    // [1]. check Switch all case complexity level
    if (countSwCodeComplexity(swCase) > 30){
        #ifdef DEBUG
        errs()<<"[*] 复杂度特征符合!\n";
        #endif
        return true;
    }
    // [2]. check Switch all case's function called features
    map<StringRef,int> prefixCallFuncMap,suffixCallFuncMap;
    map<StringRef,int> functionCountMap; //辅助：避免出现每个case调用同一个函数的情况

    for (auto &onecase : swCase)
    {
        map<StringRef,int> prefixTmpCountMap,suffixTmpCountMap;  //如果一case中同时出现handleA和handleB，在本轮中map的计数只取1，换言之，对整个switch的统计而言，处理每个case分支时，其调用次数需是唯一的
        map<StringRef,int> functionTmpCountMap;

        for (Function::iterator begin_BB(onecase.beginBB),end_BB(onecase.endBB); begin_BB != end_BB;begin_BB++)
        {
            BasicBlock *basicblock = &*begin_BB;
            for(Instruction &instruction: *basicblock){
                // errs()<<"BB: "<<basicblock->getName()<<"\n";
                // errs()<<"instruction: "<<instruction<<"\t"<<&instruction<<"\n";

                // if (!isa<CallInst>(&instruction))
                // {
                //     continue;
                // }

                CallSite callsite(&instruction);
                if (!callsite)
                {
                    continue;
                }
                
                if (Function *function = callsite.getCalledFunction()) // only support direct call 
                {
                    if (function->isDeclaration())
                    {
                        continue;
                    }
                    StringRef funcname;
                    if (PROGRAM_LANG_FLAGS)
                    {
                        funcname = function->getSubprogram()->getName();
                    }else{
                        funcname = function->getName();
                    }
                    #ifdef DEBUG
                    errs()<<"当前case调用的方法: "<<funcname<<"\n";
                    #endif

                    // store prefix info
                    if (prefixTmpCountMap.find(funcname.substr(0,4)) == prefixTmpCountMap.end())
                    {
                        // errs()<<"准备记录的函数: "<<funcname.substr(0,4)<<"\n";
                        prefixTmpCountMap[funcname.substr(0,4)] = 1;
                        prefixCallFuncMap[funcname.substr(0,4)]++;
                    }
                    // store suffix info
                    if (suffixTmpCountMap.find(funcname.substr(funcname.size()-4)) == suffixTmpCountMap.end())
                    {
                        suffixTmpCountMap[funcname.substr(funcname.size()-4)] = 1;
                        suffixCallFuncMap[funcname.substr(funcname.size()-4)]++;
                    }
                    if (functionTmpCountMap.find(funcname) == functionTmpCountMap.end())
                    {
                        functionTmpCountMap[funcname] = 1;
                        functionCountMap[funcname]++;
                    }
                    
                }
            }
        }
    }
    // [各case调用的函数相似] 检查调用函数的前后缀符合达标80%这个标准...(需避免一模一样的情况，例如各case均调用相同的函数：log_printf()，这种属于干扰项)
    // 1. check prefix && check suffix
    if (checkSwitchCaseCallTimes(swCase.size(),prefixCallFuncMap,functionCountMap) || checkSwitchCaseCallTimes(swCase.size(),suffixCallFuncMap,functionCountMap))
    {
        #ifdef DEBUG
        errs()<<"[*] 调用特征符合!\n";
        #endif
        return true;
    }
    // [各case调用的函数完全不同] 检查调用函数的唯一性是否满足80%这一标准
    if (functionCountMap.size() >= 0.8*swCase.size()){
        #ifdef DEBUG
        errs()<<"[*] 调用了乱七八糟的函数, 特征符合!\n";
        #endif
        return true;
    }

    return false;
}

bool cmp(pair<StringRef,int>&a,pair<StringRef,int>&b){
    return a.second > b.second;
}

bool cmp_int_int(pair<int,int>&a,pair<int,int>&b){
    return a.second > b.second;
}

template<typename type1,typename type2>
bool cmp2(pair<type1,type2>&a,pair<type1,type2>&b){
    return a.second > b.second;
}

/**
 * @description: 检查调用函数的前后缀的次数是否符合80%的size
 * @param {int} swCaseSize
 * @param {map<StringRef,int> CountMap,map<StringRef,int>} functionCountMap
 * @return {*}
 */
bool checkSwitchCaseCallTimes(int swCaseSize,map<StringRef,int> CountMap,map<StringRef,int> functionCountMap){

    #ifdef DEBUG
    errs()<<"[checkSwitchCaseCallTimes]\n";
    #endif

    vector<pair<StringRef,int>> CountMapVec;
    CountMapVec.assign(CountMap.begin(),CountMap.end());

    std::sort(CountMapVec.begin(),CountMapVec.end(),cmp);
    #ifdef DEBUG
    errs()<<"CountMapVec size: "<<CountMap.size()<<"\t"<<functionCountMap.size()<<"\n";
    #endif
    for (auto one : CountMapVec)
    {
        bool retVal = true;
        #ifdef DEBUG
        errs()<<"one.second: "<<one.second<<"\tlength: "<<swCaseSize<<"\n";
        #endif
        if (one.second >= swCaseSize*0.7)
        {
            //check if one.first is the same in all case
            for (auto function : functionCountMap)
            {
                if ((function.first.startswith(one.first) || function.first.endswith(one.first)) && function.second >= swCaseSize*0.8)  //某个函数出现的次数  >= swCaseSize*0.8  ，即 每个case调用了相同的函数，不应考虑
                {
                    retVal = false;
                    break;
                }
            }
            return retVal;
        }
    }
    return false;
}
/**
 * @description: 提取Swtich的所有Case,获取各Case的起始和结尾基本块,以及Case所在的基本块个数
 * @param {Instruction} *instruction
 * @param {vector<SwitchCase>} &swCase
 * @return {*}
 */
void extractAllCase(Instruction *instruction,vector<SwitchCase> &swCase,int &error){
    SwitchInst *swInst = cast<SwitchInst>(instruction);
    vector<BasicBlock*> tmpVec;
    
    for (auto &swCase : swInst->cases())
    {
        tmpVec.push_back(swCase.getCaseSuccessor());
        #ifdef DEBUG
        errs()<<"case: "<<swCase.getCaseSuccessor()->getName()<<"\n";
        #endif
    }
    #ifdef DEBUG
    errs()<<"default: "<<swInst->getDefaultDest()->getName()<<"\n";
    #endif
    // errs()<<"epilog: "<<getSwitchEnd(swInst)->getName()<<"\n";
    for (size_t i = 0; i < tmpVec.size(); i++)
    {
        SwitchCase oneCase;
        oneCase.beginBB = tmpVec[i];

        if (i == tmpVec.size()-1)
        {
            oneCase.endBB = getSwitchEnd(swInst,tmpVec[i]);
            #ifdef DEBUG
            errs()<<"beginBB: "<<oneCase.beginBB->getName()<<"\tendBB: "<<oneCase.endBB<<"\n";
            #endif

            if (oneCase.endBB == NULL)
            {
                error = 1;
                assert(1==2);
                return;
            }
            
        }else{
            oneCase.endBB = tmpVec[i+1];
        }
        swCase.push_back(oneCase);        
    }
    // errs()<<"switch size: "<<swCase.size()<<"\n";
    for (auto &onecase : swCase)
    {
        #ifdef DEBUG
        errs()<<"start: "<<onecase.beginBB->getName()<<"\tend: "<<onecase.endBB->getName()<<"\tsize: "<<calculateCaseBBSize(onecase.beginBB,onecase.endBB)<<"\n";
        #endif
        onecase.BBnum = calculateCaseBBSize(onecase.beginBB,onecase.endBB);
    }
}
/**
 * @description: 统计Case的BasicBlock个数
 * @param {BasicBlock} *start
 * @param {BasicBlock} *end
 * @return {*}
 */
int calculateCaseBBSize(BasicBlock *start,BasicBlock *end){
    int number = 0;
    for (Function::iterator iter_start(start),iter_end(end);iter_start != iter_end; iter_start++)
    {
        if (!iter_start->getName().startswith(StringRef("sw.default"))){
            number++;
        }
    }
    return number;
}
/**
 * @description: 获取Switch的末尾块，如sw.epilog，没有则以default，还是没有则考虑return
 * @param {SwitchInst} *swInst
 * @return {*}
 */
BasicBlock* getSwitchEnd(SwitchInst *swInst,BasicBlock *basicblock){
    Function *function = swInst->getFunction();
    BasicBlock *curBasicBlock = basicblock;

    for (Function::iterator b(curBasicBlock); b != function->end();b++)
    {
        BasicBlock *basicblock = &*b;
        if (basicblock->getName().startswith(StringRef("sw.epilog")))
        {
            return basicblock;
        }
    }
    for (Function::iterator b(curBasicBlock); b != function->end();b++)
    {
        BasicBlock *basicblock = &*b;
        if (basicblock->getName().startswith(StringRef("sw.default")))
        {
            return basicblock;
        }
    }
    for (Function::iterator b(curBasicBlock); b != function->end();b++)
    {
        BasicBlock *basicblock = &*b;
        if (basicblock->getName().startswith(StringRef("return")))
        {
            return basicblock;
        }
    }
    

    // assert(1==2);
    return NULL;
}

/**
 * @description: 提取Switch condition的语义名称
 * @param {Instruction} *Inst
 * @return {*}
 */
string extractSwitchSemantics(Instruction *Inst){
    // BasicBlock *curBasicBlock = Inst->getParent();
    // errs()<<"[Inst] "<<*Inst<<"\n";

    if (isa<LoadInst>(Inst) && !Inst->getOperand(0)->getName().contains(StringRef("arrayidx")))
    {
        Value *variable = Inst->getOperand(0);
        string variableName = getVariableNameWithParent(variable);   //TODO:增加考虑，如果是msg->state形式
        // errs()<<"switch condition semantic: "<<variableName<<"\n";

        // return delLLVMIRInfoForVar(Inst->getOperand(0)->getName());
        return variableName;
    }

    if (isa<SwitchInst>(Inst))
    {
        SwitchInst *switchInst = cast<SwitchInst>(Inst);
        CallInst *callInst = dyn_cast<CallInst>(switchInst->getCondition());
        if (callInst)
        {
            if (isa<Function>(callInst->getCalledValue()))
            {
                Function *function = cast<Function>(callInst->getCalledValue());
                if (function->isDeclaration()==0 && function->getSubprogram() != NULL)
                {
                    return delLLVMIRInfoForVar(function->getSubprogram()->getName()).str();
                }
            }
        }
    }

    queue<Instruction*> fqueue;
    fqueue.push(Inst);
    while (fqueue.size()!=0)
    {
        Instruction *tmpInst = fqueue.front();
        fqueue.pop();
        //遍历指令操作数
        for (auto &op : tmpInst->operands())
        {
            if (Instruction *op_inst = dyn_cast<Instruction>(op))
            {
                //若是load指令，且操作数非arrayidx，则返回
                if (isa<LoadInst>(op_inst) && !op_inst->getOperand(0)->getName().contains(StringRef("arrayidx")))
                {
                    Value *variable = op_inst->getOperand(0);
                    string variableName = getVariableNameWithParent(variable);
                    // errs()<<"variableName: "<<variableName<<"\t"<<*variable<<"\n";
                    // return delLLVMIRInfoForVar(op_inst->getOperand(0)->getName());
                    return variableName;
                }
                fqueue.push(op_inst);
            }
        }
    }
    return "";
}

StringRef extractFunctionArgSemantics(Value *argval){
    StringRef argsname;
    vector<StringRef> blackList = {StringRef(""),StringRef("arraydecay"),StringRef("arrayidx")};
    if (GEPOperator *gepOp = dyn_cast<GEPOperator>(argval))
    {
        argsname = delLLVMIRInfoForVar(gepOp->getOperand(0)->getName());
    }else if (Instruction *argInst = dyn_cast<Instruction>(argval))
    {
        argsname = delLLVMIRInfoForVar(argInst->getName());
        //如果argsname为空或者是索引之类的，则继续向上提取
        queue<Instruction*> fqueue;
        if (find(blackList.begin(),blackList.end(),argsname) != blackList.end())
        {
            fqueue.push(argInst);
            
            while (fqueue.size() != 0)
            {
                Instruction *inst = fqueue.front();
                fqueue.pop();

                for (size_t i = 0; i < inst->getNumOperands(); i++)
                {
                    Instruction *tmpInst = dyn_cast<Instruction>(inst->getOperand(i));
                    if (tmpInst != NULL){
                        argsname = delLLVMIRInfoForVar(tmpInst->getName());
                        if (find(blackList.begin(),blackList.end(),argsname) != blackList.end())
                        {
                            fqueue.push(tmpInst);
                            continue;
                        }else{
                            return argsname;
                        }
                    }
                }
            }
        }
    }
    return argsname;
}

void extractIFObjInCurFunction(Function *function,vector<IfConditionObj> &AllIfObjInCurFunc){
    Function::iterator bbIter = function->begin();
    int IfObjNumber = 1;

    while (bbIter != function->end())
    {
        BasicBlock *basicblock = &*bbIter;
        // check cur basicblock is If Statement , especially beginning
        if (checkIsIF(basicblock)){
            vector<BasicBlock*> resultVec;
            if (checkSuccStartWithIfNumber(basicblock)==2)
            {
                // errs()<<basicblock->getName()<<"\t是完整的IF Condition\n";
                resultVec.push_back(basicblock);
                // 寻找为true包裹的代码块
                vector<BasicBlock*> rangeBB(2);
                extractBranchTrue(basicblock,rangeBB);
                // 保存为一个结构体至vector中
                storeTheIfObj(AllIfObjInCurFunc,resultVec,rangeBB,IfObjNumber);
                IfObjNumber++;
            }
            //如果有一个不是if开头，则遍历取处理它,构造成完整的if表达式
            else if (checkSuccStartWithIfNumber(basicblock)==1)
            {
                // errs()<<basicblock->getName()<<"是IF Condition的开端~\n";
                // errs()<<"没有以if开头的是: "<<getNotStartWithIfBasicBlock(basicblock)->getName()<<"\n";
                traverseForIFCondition(basicblock,resultVec);

                BasicBlock *endBB =  resultVec[resultVec.size()-1];                

                // 寻找为true包裹的代码块
                vector<BasicBlock*> rangeBB(2);
                extractBranchTrue(endBB,rangeBB);
                // errs()<<"真正的结尾块: "<<endBB->getName()<<"\n";
                // errs()<<"开始: "<<rangeBB[0]->getName()<<"\t结束: "<<rangeBB[1]->getName()<<"\n";

                // 保存为一个结构体至vector中
                storeTheIfObj(AllIfObjInCurFunc,resultVec,rangeBB,IfObjNumber);
                IfObjNumber++;
                // 跳过同一表达式中的BasicBlocks
                Function::iterator Iter(endBB);
                bbIter = Iter;
            }
        }
        bbIter++;
    }
}

/** 这里可以需要注意一点啊，状态机变量我们是有一个约束范围的，在这个范围的才考虑，这是为了避免状态爆炸，记住。，所以这里可以搞一个这个来补充前面的handleSwitch，但是必须加约束范围。
 * @description: 泛化阶段，提取switch中的关键变量。捞一下switch中的状态和子状态变量。然后需要删除handlwSwtich中处理子状态变量的逻辑
 * @param {Module} &M
 * @param {map<Function*,int> CallGraphMap,
 * @param vector<vector<string>> MeaningDict,
 * @param vector<Instruction*>} AlreadyMonitorVarVec
 * @return {*}
 */
void handleNormalSwitch(Module &M,map<Function*,int> CallGraphMap,vector<vector<string>> MeaningDict){
    vector<SwitchInstrumentObj> switchResults;  //保存组合类型的候选Switch对象
    vector<SwitchInstrumentObj> switchStateMachineResults;  //保存状态机级别的候选Swtich对象

    for (auto &function : M.getFunctionList())
    {
        if (CallGraphMap.find(&function) == CallGraphMap.end() || function.isDeclaration() || function.getSubprogram()==NULL)
            continue;
        for (inst_iterator Inst = inst_begin(function),End = inst_end(function);Inst != End; ++Inst)
        {
            Instruction *instruction = cast<Instruction>(&*Inst);
            if (isa<SwitchInst>(instruction) && instruction->getDebugLoc().get() != NULL)
            {
                SwitchInst *swInst = cast<SwitchInst>(instruction);
                if (!isa<Instruction>(swInst->getCondition()))
                {
                    continue;
                }
                // errs()<<"AlreadyMonitorVarVec.size: "<<AlreadyMonitorVarVec.size()<<"\n";
                
                // 1. 提取condition语义
                string semantics = extractSwitchSemantics(instruction);
                vector<SwitchCase> swCase;
                int error = 0;
                extractAllCase(instruction,swCase,error);
                if (swCase.size() < 3)
                {
                    continue;
                }
                
                // // 严格匹配检查是否状态机变量   TODO:容易把根据string赋值这种函数弄进来，如果不考虑结构的话。
                // if (checkArgsMeaningIfOK(MeaningDict,semantics,New_StateMachine_Args_Pos,1) && checkFuncInStateMachineCallGraph(function.getSubprogram()->getFilename()))
                // {
                //     extractSwCaseValue(swInst,stateMachineAssistInfo.SwCaseStateVal);   //提取case val
                //     SwitchInstrumentObj obj;
                //     obj.swinst = swInst;
                //     obj.swcondition = cast<Instruction>(swInst->getCondition());
                //     obj.swCase = swCase;
                //     obj.semantics = semantics;
                //     stateMachineAssistInfo.VarType.push_back(identifyStateVarType(obj.swcondition));    //保存协议状态机变量的类型
                //     switchStateMachineResults.push_back(obj);
                // }else
                if (checkArgsMeaningIfOK(MeaningDict,semantics,Protocol_Args_Pos,0))   //有子状态语义
                {
                    SwitchInstrumentObj obj;
                    obj.swinst = swInst;
                    obj.swcondition = cast<Instruction>(swInst->getCondition());
                    obj.swCase = swCase;
                    obj.semantics = semantics;
                    switchResults.push_back(obj); 
                }else if (checkCaseFeatures(swCase) && error==0){   //有结构，无语义
                    SwitchInstrumentObj obj;
                    obj.swinst = swInst;
                    obj.swcondition = cast<Instruction>(swInst->getCondition());
                    obj.swCase = swCase;
                    obj.semantics = semantics;
                    MeaningDict[Protocol_Args_Pos].push_back(semantics); //子状态自学习
                    // 保存为候选目标，后续检查调用函数以进一步的筛选
                    switchResults.push_back(obj); 
                }
            }
        }
    }
    handleCandidateStateMachineSwitch(switchStateMachineResults);
    handleCandidateSwitchObj(switchResults,MeaningDict);
}

int checkSingleIfCodeComplexity(IfConditionObj ifObjPtr,int complexity) {
    
    return 1;

    int count = 0;
    int functioncall = 0;
    BasicBlock *beginBB = ifObjPtr.branchTrueBeginBB;
    BasicBlock *endBB = ifObjPtr.branchTrueEndBB;

    for (Function::iterator beginIter(beginBB), endIter(endBB); beginIter != endIter; beginIter++)
    {
        count++;
        BasicBlock *basicblock = &*beginIter;
        for (Instruction &instruction : *basicblock)
        {
            CallInst *callInst = dyn_cast<CallInst>(&instruction);
            {
                if (callInst)
                {
                    Function *function = callInst->getCalledFunction();
                    // errs()<<"function: "<<*callInst<<"\t"<<(function==NULL)<<"\n";
                    // errs()<<"name "<<<<"\n";
                    if (function != NULL && !function->isDeclaration())
                    {
                        functioncall++;
                    }
                }
            }
        }
    }
    //要求：if code中要有函数调用，或者复杂度要够多>20
    if (functioncall != 0 && count < 10)
        return 1;
    if (count < complexity)
        return 0;
    return 1;
}