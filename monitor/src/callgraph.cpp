#include "callgraph.h"
#include "funcpointer.h"
#include "../include/cppheader.h"

using namespace std;
using namespace llvm;

extern int PROGRAM_LANG_FLAGS;

/**
 * @description: 提取Module中所有调用了read()...和accept()...的函数
 * @param {Module} &M
 * @param {vector<Function*> &readFuncVec: 保存调用read()...的函数
 * @param vector<Function*>} &acceptFuncVec: 保存调用accept()的函数
 * @return {*}
 */
void extractAllRecvAndAccept(Module &M,vector<Function*> &readFuncVec,vector<Function*> &acceptFuncVec){
    bool isUDP = checkIfTCPorUDP();

    vector<StringRef> readFunctionDic = {"read","recv","recvfrom","recvmsg"};
    int count = 0;

    for (auto &function:M.getFunctionList())
    {
        if (function.isDeclaration())
        {
            continue;
        }
        count++;
        
        for (inst_iterator Inst = inst_begin(function),End = inst_end(function);Inst != End; ++Inst)
        {
            Instruction *instruction = cast<Instruction>(&*Inst);
            CallSite callsite(instruction);
            if (callsite)
            {
                // 1.call @function
                if (isa<Function>(callsite.getCalledValue()))
                {
                    Function *funcalled = callsite.getCalledFunction();
                    if (!funcalled->isDeclaration())
                    {
                        continue;
                    }
                    
                    if (isUDP==false && funcalled->getName().equals("accept"))
                    {
                        acceptFuncVec.push_back(&function);
                        break;
                    }else if (find(readFunctionDic.begin(),readFunctionDic.end(),funcalled->getName())!=readFunctionDic.end())
                    {
                        readFuncVec.push_back(&function);
                        break;
                    }
                }else if (isa<Instruction>(callsite.getCalledValue()))
                {
                    // 2.call 函数指针，案例：proftpd - pr_netio_read()
                    Instruction *instruction = dyn_cast<Instruction>(callsite.getCalledValue());
                    if (instruction)
                    {
                        StringRef funcname = instruction->getOperand(0)->getName();
                        //解决出现read42这种情况：只保留函数名称
                        int i;
                        for(i = funcname.size()-1; i>=0;i--){
                            if(!(funcname[i]>='0'&&funcname[i]<='9')){
                                break;
                            }
                        }
                        if (i != 0)
                        {
                            StringRef funcName = funcname.substr(0,i+1);
                            if (find(readFunctionDic.begin(),readFunctionDic.end(),funcName)!=readFunctionDic.end())
                            {
                                readFuncVec.push_back(&function);
                                break;
                            }else if (funcName.equals(StringRef("accept")))
                            {
                                acceptFuncVec.push_back(&function);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    errs()<<"一共函数个数是: "<<count<<"\n";

    errs()<<"readFuncVec size: "<<readFuncVec.size()<<"\n=====\n";
    for (Function *func:readFuncVec)
    {
        errs()<<func->getName()<<"\n";
    }
    
    errs()<<"acceptFuncVec size: "<<acceptFuncVec.size()<<"\n=====\n";
    for (Function *func:acceptFuncVec)
    {
        errs()<<func->getName()<<"\n";
    }
    errs()<<"=====\n";
}
/**
 * @description: 从TargetFunc开始向上回溯maxBackTraceLevel层,结果保存至AllParentFuncVec
 * @param {Function} *TargetFunc
 * @param {vector<vector<Function*>>} &AllParentFuncVec
 * @param {int} maxBackTraceLevel
 * @return {*}
 */
void backtraceForTargetFunc(Function *TargetFunc,vector<vector<Function*>> &AllParentFuncVec,int maxBackTraceLevel){
    queue<vector<Function*>> ffqueue;
    vector<Function*> ParentFunc;
    ParentFunc.push_back(TargetFunc);
    ffqueue.push(ParentFunc);

    int ccurPos = 0;
    while (ccurPos < maxBackTraceLevel && !ffqueue.empty())
    {
        vector<Function*> tmp = ffqueue.front();

        ffqueue.pop();
        backtraceOneLevel(tmp,AllParentFuncVec,ffqueue,ccurPos);        
        ccurPos++;
        // errs()<<"AllParentFuncVec size: "<<AllParentFuncVec.size()<<"\n";
    }
    // //FIXME:debug
    // if (TargetFunc->getName().equals(StringRef("pr_netio_read")))
    // {
    //     for (auto line:AllParentFuncVec)
    //     {
    //         errs()<<"行:\t";
    //         for (auto col : line)
    //         {
    //             errs()<<col->getName()<<"\t";
    //         }
    //         errs()<<"\n";
    //     }
    // }
}
/**
 * @description: 检查全局变量globalvar是否作为函数被调用
 * @param {GlobalVariable} *globalvar
 * @return {*}
 */
void checkGlobalVariablesIfCall(GlobalVariable *globalvar,vector<Function*> &resVec){
    Value *glval = globalvar;
    for (auto uii : glval->users()){
        //situation1 proftpd
        //:store void (%struct.server_struc*, %struct.conn_struc*)* @cmd_loop, void (%struct.server_struc*, %struct.conn_struc*)** @cmd_handler, align 8
        //%150 = load void (%struct.server_struc*, %struct.conn_struc*)*, void (%struct.server_struc*, %struct.conn_struc*)** @cmd_handler, align 8, !dbg !14914
        //call void %150(%struct.server_struc* %151, %struct.conn_struc* %152), !dbg !12512
        if (LoadInst *loadinst = dyn_cast<LoadInst>(uii))
        {
            Value *loadval = loadinst;
            for (auto uiii:loadval->users())
            {
                if (CallInst *callinst = dyn_cast<CallInst>(uiii))
                {
                    if (callinst->getOperand(callinst->getNumOperands()-1)==loadinst)
                    {
                        resVec.push_back(callinst->getFunction());
                    }
                }
            }
        }
    }
}

/**
 * @description: 以functionVec为起点向上回溯一层（backtraceForTargetFunc()辅助函数）
 * @param {vector<Function*> functionVec: 回溯的开端
 * @param vector<vector<Function*>> &AllParentFuncVec: 逐层保存回溯的函数
 * @param queue<vector<Function*>>} &fqueue
 * @return {*}
 */
void backtraceOneLevel(vector<Function*> functionVec,vector<vector<Function*>> &AllParentFuncVec,queue<vector<Function*>> &fqueue,int ccurpos){
    vector<Function*> parentFuncVec;
    map<Function*,int> parentFuncVec_tmpmap;//辅助parentFuncVec，避免重复插入

    //iterate the beginning og the backtrace function
    for (auto function : functionVec)
    {
        //check which instructions use these function
        for (Value::use_iterator ui = function->use_begin(); ui != function->use_end(); ui++)
        {
            // //FIXME:debug
            // if (function->getName().equals(StringRef("cmd_loop")))
            // {
            //     errs()<<"cmd_loop use: "<<*ui->getUser()<<"\n";
            // }
            //
            /*
            proftpd中有如下指针调用的情况，store指令->赋值对象是全局变量，则遍历检查全局变量在哪里调用，保存对应的父函数
                此处的特征是load这个全局变量，然后call
            */
            if (Instruction *call = dyn_cast<Instruction>(ui->getUser()))
            {
                CallSite callsite(call);
                if (callsite)
                {
                    // 显示调用了该函数
                    Function *called = callsite->getFunction();
                    if (parentFuncVec_tmpmap.find(called) == parentFuncVec_tmpmap.end())
                    {
                        parentFuncVec_tmpmap.insert(pair<Function*,int>(called,0));
                        parentFuncVec.push_back(called);
                    }                    
                }else if (StoreInst *storeinst = dyn_cast<StoreInst>(call))
                {
                /*  proftpd cmd_loop案例
                    store void (%struct.server_struc*, %struct.conn_struc*)* @cmd_loop, void (%struct.server_struc*, %struct.conn_struc*)** @cmd_handler, align 8
                 */
                    if (isa<Function>(storeinst->getOperand(0)))
                    {
                        GlobalVariable *gv = dyn_cast<GlobalVariable>(storeinst->getOperand(1));
                        if (gv)
                        {
                            vector<Function*> resVe;
                            checkGlobalVariablesIfCall(gv,resVe);
                            for (auto func : resVe)
                            {
                                parentFuncVec_tmpmap.insert(pair<Function*,int>(func,0));
                                parentFuncVec.push_back(func);
                            }
                        }
                    }
                }
            }else{
                // 该函数被用于某一个函数的参数中，如proftpd中由pthread_create调用ftp_client_thread()
                User *user = ui->getUser();
                for (auto usr : user->users())
                {
                    if (CallInst *call = dyn_cast<CallInst>(usr))
                    {
                        if (parentFuncVec_tmpmap.find(call->getFunction()) == parentFuncVec_tmpmap.end())
                        {
                            parentFuncVec_tmpmap.insert(pair<Function*,int>(call->getFunction(),0));
                            parentFuncVec.push_back(call->getFunction());
                        } 
                    }
                }
            }
        }
    }
    // if (ccurpos!=0)
    // {
        
    // }
    
    if (parentFuncVec.size()!=0)
    {
        fqueue.push(parentFuncVec);
        AllParentFuncVec.push_back(parentFuncVec);
    }    
}
/**
 * @description: 将二维数组（Function*）转化为一维的map<Function*,int>
 * @param {vector<vector<Function*>> AllParentRecvFuncVec,map<Function*,int>} &resultMap
 * @return {*}
 */
void matrixToMap(vector<vector<Function*>> AllParentRecvFuncVec,map<Function*,int> &resultMap){
    int level = 0;
    for (auto row : AllParentRecvFuncVec)
    {
        for (auto col : row)
        {
            if (resultMap.find(col) == resultMap.end())
            {
                resultMap.insert(pair<Function*,int>(col,level));  
            }
        }
        level++;
    }
}

/**
 * @description: 过滤获取到的read()...和accept()回溯的交叉函数结点，如继续向上回溯5层，判断是否能够回溯至main函数
 * @param {Module} &M
 * @param {vector<Function*>} &MergeNodeVec 待检测的交叉函数点
 * @return {*}
 */
void filterMergeNodeVec(Module &M,vector<Function*> &MergeNodeVec,vector<pair<Function*,int>> &MergeNodeVec_level){
    uniqueVector(MergeNodeVec);
    uniqueVector(MergeNodeVec_level);
    
    Function *MainFunc = M.getFunction(StringRef("main"));
    assert(MainFunc);
    //遍历交叉函数结点，向上回溯判断是否能够到达main函数
    auto iter = MergeNodeVec.begin();
    while(iter != MergeNodeVec.end())
    {
        if (*iter == MainFunc)
        {
            iter++;
            continue;
        }
        
        vector<vector<Function*>> AllParentFuncVec;
        vector<Function*> result;
        Function *func = *iter;
        backtraceForTargetFunc(func,AllParentFuncVec,5);
        
        map<Function*,int> resultMap;
        matrixToMap(AllParentFuncVec,resultMap);
        
        if (resultMap.find(MainFunc) == resultMap.end())
        {
            iter = MergeNodeVec.erase(iter);
        }else{
            iter++;
        }
    }
    //删除MergeNodeVec_level中无关元素
    auto iter2 = MergeNodeVec_level.begin();
    while (iter2 != MergeNodeVec_level.end())
    {
        if (find(MergeNodeVec.begin(),MergeNodeVec.end(),iter2->first) == MergeNodeVec.end())
        {
            iter2 = MergeNodeVec_level.erase(iter2);
        }else{
            iter2++;
        }
    }
}
/**
 * @description: 从指定函数开始向下遍历提取Call function,保存至map<Function*,int>中
 * @param {Function} *SourceFunc 遍历起始函数
 * @param {map<Function*,int>} &resultsMap 保存结果
 * @param {int} maxLevel 指定向下遍历的层数
 * @return {*}
 */
void iterateCallSiteFromTargetFunc(Function *SourceFunc,map<Function*,int> &resultsMap,int maxLevel){
    int level = 0;
    queue<vector<Function*>> fqueue;
    vector<Function*> ChildFunc;
    ChildFunc.push_back(SourceFunc);
    fqueue.push(ChildFunc);
    resultsMap.insert(pair<Function*,int>(SourceFunc,level));

    while (level <= maxLevel && !fqueue.empty())
    {
        vector<Function*> tmp = fqueue.front();
        fqueue.pop();

        //循环提取遍历tmp中的函数
        level++;
        iterateOneLevel(tmp,resultsMap,level,fqueue);
    }
}
/**
 * @description: 检查函数中是否有call指令
 * @param {Function} *function
 * @return {*}
 */
bool checkFunctionIfContainCallInst(Function *function){
    for (inst_iterator Inst = inst_begin(function),End = inst_end(function);Inst != End; ++Inst)
    {
        Instruction *instruction = cast<Instruction>(&*Inst);
        CallSite callsite(instruction);
        if (callsite)
        {
            return true;
        }
    }
    return false;
}

/**
 * @description: 由指定函数数组开始向下遍历指定层
 * @param {vector<Function*> tmpvec,
 * @param map<Function*,int> &resultsMap,
 * @param int level,
 * @param queue<vector<Function*>>} &fqueue
 * @return {*}
 */
void iterateOneLevel(vector<Function*> beginVec,map<Function*,int> &resultsMap,int level,queue<vector<Function*>> &fqueue){
    vector<Function*> tmpresults;   //暂存本轮获取的信息，将存入队列中

    for (auto func : beginVec)
    {   
        if (checkIfBlackList(func->getName().lower())){
            continue;
        }
        if (func->isDeclaration() || (checkFunctionIfContainCallInst(func)==false && func->getBasicBlockList().size()<5) )  //将proftpd中的案例给过滤掉了...so 增加检测函数内部有无call指令，如果没有call且小于5，才视为...
        {
            continue;
        }
        // errs()<<"function: "<<func->getName()<<"\n";
        //提取函数
        for (inst_iterator Inst = inst_begin(func),End = inst_end(func);Inst != End; ++Inst)
        {
            Instruction *instruction = cast<Instruction>(&*Inst);
            CallSite callsite(instruction);
            // errs()<<"instruction "<<*instruction<<"\n";
            if (callsite)
            {
                // [situation1 : called function has casted] , e.g. call void (...) bitcast (void ()* @expand_groups to void (...)*)()
                if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction->getOperand(0)))
                {
                    if (cstexpr->isCast())
                    {
                        Function *function = dyn_cast<Function>(cstexpr->stripPointerCasts());
                        if (function && function->isDeclaration()==0)
                        {
                            resultsMap.insert(pair<Function*,int>(function,level));
                            tmpresults.push_back(function);
                        }else{
                            continue;
                        }
                    }
                }
                else
                {
                    // [situation2 : call function pointer] , e.g. call void %12(i8* %13)
                    if (isa<Instruction>(callsite.getCalledValue())) //FuncPointerVec.size()>0 即默许函数指针的源头是一个数组，必须有这种数组，才检测这种情况
                    {
                        //2-1 如果是一个全局变量，存储函数指针，则遍历Module检查这个全局变量的赋值情况,Store指令，保存Store的另一个操作数
                        // if (func->getName().equals(StringRef("fork_server")))
                        // {
                        // errs()<<"inst "<<*callsite.getCalledValue()<<"\n";
                        int funcPos = instruction->getNumOperands()-1;
                        Instruction *loadInst = dyn_cast<Instruction>(instruction->getOperand(funcPos));
                        if (loadInst && isa<GlobalVariable>(loadInst->getOperand(0)))
                        {
                            GlobalVariable *FuncGV = cast<GlobalVariable>(loadInst->getOperand(0));
                            //遍历Module，提取GlobalVariables的赋值情况
                            vector<Function*> functionVec;
                            extractGVFuncPointerInitialInfo(loadInst->getModule(),FuncGV,functionVec);
                            for (auto func_tmp:functionVec)
                            {
                                // errs()<<func->getName()<<"\n";
                                tmpresults.push_back(func_tmp);
                                resultsMap.insert(pair<Function*,int>(func_tmp,level));
                            }
                        }else if(GlobalVariable *globalArray = checkCalledObjIfPointToGlobalArray(loadInst)){
                            //2-2 如果call的load指令来源于全局数组，则遍历这个全局数组，获取其中的存储的函数地址
                            // 遍历globalArray
                            vector<Function*> storedVec;
                            iterateGlobalArrayToStoreFunction(globalArray,storedVec);
                            for (auto function:storedVec)
                            {
                                tmpresults.push_back(function);
                                resultsMap.insert(pair<Function*,int>(function,level));
                            }
                        }
                        // errs() << "函数指针调用: " << *instruction << "\t" << instruction->getNumOperands() << "\n" << *instruction->getOperand(funcPos) << "\n";
                        // Instruction *load = dyn_cast<Instruction>(instruction->getOperand(2));
                        // errs()<<*load->getOperand(0)<<"\n";
                    }//TODO:2-2如果指向一个全局指针数组，则遍历这个全局数组，获取其中的存储的函数地址
                    
                    // }
                    else if (isa<Function>(callsite.getCalledValue()))
                    {
                        // errs()<<"**Inst**"<<*instruction<<"\n";
                        // [situation3 : call function], e.g. call void (i8, i8*, ...) @control_printf(...)
                        if ( Function *function = dyn_cast<Function>(callsite.getCalledValue()))
                        {
                            if (function->isDeclaration()==0)
                            {
                                resultsMap.insert(pair<Function*,int>(function,level));
                                tmpresults.push_back(function);
                            }
                        }
                        // resultsMap.insert(pair<Function*,int>(callsite.getCalledFunction(),level));
                        // tmpresults.push_back(callsite.getCalledFunction());
                        //如果函数的参数有函数地址，遍历一下检查是否有函数，例如pthread_create()参数可能有触发其他函数的操作
                        Use *args = callsite.getInstruction()->getOperandList();
                        for (size_t i = 0; i < callsite.getInstruction()->getNumOperands()-1; i++)
                        {
                            // 函数参数直接是Function
                            if (Function *callfunc = dyn_cast<Function>(args[i].get()))
                            {
                                if (callfunc->isDeclaration()==0)
                                {
                                    resultsMap.insert(pair<Function*,int>(callfunc,level));
                                    tmpresults.push_back(callfunc);
                                }
                            }else if (auto *cstexpr = dyn_cast<ConstantExpr>(args[i].get()))
                            {
                                // 函数参数是bitcast的，需要剥离一下
                                Function *function = dyn_cast<Function>(cstexpr->stripPointerCasts());
                                if (function && func->isDeclaration()==0)
                                {
                                    resultsMap.insert(pair<Function*,int>(function,level));
                                    tmpresults.push_back(function);
                                }
                            }
                        }
                    }
                }
            }
            //TODO:如果指令是GEP指令，且操作的目标是存放了函数地址的全局数组，则意味着此处可能存在调用其他函数的逻辑，应当将其纳入callgraph范围
            if (GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(instruction)){
                Instruction *gepObj = dyn_cast<Instruction>(gepInst->getOperand(0));
                if (gepObj){
                    Value *obj = gepObj->getOperand(0);
                    if (GlobalVariable *globalArray = dyn_cast<GlobalVariable>(obj))
                    {
                        // errs()<<"找到隐式调用: "<<*globalArray<<"\n";
                        vector<Function*> storedVec;
                        iterateGlobalArrayToStoreFunction(globalArray,storedVec);
                        for (auto function:storedVec)
                        {
                            tmpresults.push_back(function);
                            resultsMap.insert(pair<Function*,int>(function,level));
                        }
                    }
                }
            }   
        }
    }
    if (tmpresults.size()!=0)
    {
        set<Function*> s(tmpresults.begin(),tmpresults.end());
        tmpresults.assign(s.begin(),s.end());
        fqueue.push(tmpresults);
    }
}
/**
 * @description: 提取callgraph范围
 * @param {Module} &M
 * @return {*}
 */
void extractCallGraph(Module &M,map<Function*,int> &CallGraphMap_State,map<Function*,int> &CallGraphMap_All){
    // // 若项目为C程序
    // if (!PROGRAM_LANG_FLAGS){
    //     // 判断项目是否使用了网络库，以选取不同的回溯处理方法（暂时默认是原生网络API，如read、accept）
    //     unix_network_extractCallGraph(M,CallGraphMap_State,CallGraphMap_All);
    // }else{
        //CPP程序
        extractCheckRangeForCPP(M,CallGraphMap_All);
    // }
}
/**
 * @description: 为CPP程序提取检测范围
 * @param {Module} &M
 * @param {map<Function*,int>} &CallGraphMap
 * @return {*}
 */
void extractCheckRangeForCPP(Module &M,map<Function*,int> &CallGraphMap){
    // int count = 0;
    for (auto &function:M.getFunctionList())
    {
        if (checkIfBlackList(function.getName().lower())){
            continue;
        }
        if (function.isDeclaration() || function.getBasicBlockList().size()<5 )
        {
            continue;
        }
        //析构函数、重载符号
        if (function.getSubprogram()->getName().startswith(StringRef("~")) || function.getSubprogram()->getName().startswith(StringRef("operator")))
        {
            continue;
        }
        if (function.getSubprogram()->getFilename().endswith(".h")){
    
            continue;
        }
        // if (function.getName().equals_lower("ipp_cancel_job"))
        CallGraphMap.insert(pair<Function*,int>(&function,1));
    }
}

/**
 * @description: 提取Unix network api的情景下的callgraph
 * @param {Module} &M
 * @param {map<Function*,int> &CallGraphMap_State,map<Function*,int>} &CallGraphMap_All 保存结果
 * @return {*}
 */
void unix_network_extractCallGraph(Module &M,map<Function*,int> &CallGraphMap_State,map<Function*,int> &CallGraphMap_All){

    //If user give a start function , then extract callgraph directly
    char *giveBeginFunction = getenv("START_FUNCTION");
    bool extractFlags = false;
    if (giveBeginFunction != NULL)
    {
        if (M.getFunction(StringRef(giveBeginFunction)) != NULL)
        {
            extractFlags = true;
        }
    }
    
    if (!extractFlags){
        vector<Function*> recvParentFunc,acceptParentFunc;

        //[*] extract all function which called read or accept ...
        extractAllRecvAndAccept(M,recvParentFunc,acceptParentFunc);
        // if program has no accept function -> UDP server
        if (acceptParentFunc.size()==0)
        {
            acceptParentFunc.assign(recvParentFunc.begin(),recvParentFunc.end());
        }

        vector<Function*> MergeNodeVec; //暂存recv和accept回溯的交叉函数
        vector<pair<Function*,int>> MergeNodeVec_level; //用于存储交叉函数与main函数的层数
        //用于存储read父函数与交叉函数之间的层数

        //[*] handle each recv... and accept to find their merge node
        for (auto recvFunc : recvParentFunc)
        {
            vector<vector<Function*>> AllParentRecvFuncVec;
            vector<Function*> tmp = {recvFunc};
            AllParentRecvFuncVec.push_back(tmp);
            //backtrace 4 level for recv ... function
            backtraceForTargetFunc(recvFunc,AllParentRecvFuncVec,5);
            map<Function*,int> recvMap;
            matrixToMap(AllParentRecvFuncVec,recvMap);
            
            for (auto acceptFunc : acceptParentFunc)
            {
                vector<vector<Function*>> AllParentAcceptFuncVec;
                vector<Function*> tmp = {acceptFunc};
                AllParentAcceptFuncVec.push_back(tmp);
                //backtrace 3 level for accept function
                backtraceForTargetFunc(acceptFunc,AllParentAcceptFuncVec,3);
                map<Function*,int> acceptMap;
                matrixToMap(AllParentAcceptFuncVec,acceptMap);
                
                //Now we have collected enough information to get the merge node
                //note:长度短的那个作为比较的基准，可减少比较次数
                if (recvMap.size() <= acceptMap.size())
                {
                    for (auto recvtIt : recvMap)
                    {
                        map<Function*,int>::iterator it = acceptMap.find(recvtIt.first);//交叉函数结点
                        if (it != acceptMap.end())
                        {
                            MergeNodeVec.push_back(it->first);
                            int level = calLevelBetweenTargeFuncAndOthers(M,recvFunc,it->first);//统计recvFunc与交叉函数结点相差层数
                            // errs()<<"保存:"<<it->first->getName()<<"\t"<<level<<"\n";
                            MergeNodeVec_level.push_back(pair<Function*,int>(it->first,level));
                        }
                    }
                }else if (recvMap.size() > acceptMap.size())
                {
                    for (auto acceptIt : acceptMap)
                    {
                        map<Function*,int>::iterator it = recvMap.find(acceptIt.first);//交叉函数结点
                        if (it != recvMap.end())
                        {
                            MergeNodeVec.push_back(it->first);
                            int level = calLevelBetweenTargeFuncAndOthers(M,recvFunc,it->first);
                            // errs()<<"source func:"<<recvFunc->getName()<<"\ttarget func:"<<it->first->getName()<<"\tlevel: "<<level<<"\n";
                            MergeNodeVec_level.push_back(pair<Function*,int>(it->first,level));
                        }
                    }
                }
            }
        }
        //[*] filter the MergeNode and unqieue recvFuncVec
        filterMergeNodeVec(M,MergeNodeVec,MergeNodeVec_level);

        Function *callgraphBegin;
        int StateMachineCGLevel = 5;

        if (MergeNodeVec.size()==0)
        {
            callgraphBegin = M.getFunction(StringRef("main"));
        }else{
            //[*] if MergeNode >= 1,then choose the most deep function
            callgraphBegin = chooseTheMostDeepFunction(M,MergeNodeVec,recvParentFunc,acceptParentFunc);
            StateMachineCGLevel = findStateMachineCallGraphLevel(MergeNodeVec_level,callgraphBegin);
        }

        // errs()<<"===MergeNodeVec===\n";
        // for (auto one : MergeNodeVec_level)
        // {
        //     errs()<<one.first->getName()<<"\t"<<one.second<<"\n";
        // }
        // errs()<<"===MergeNodeVec End===\n";
        // Function *callgraphBegin = chooseTheMostDeepFunction(M,MergeNodeVec,recvParentFunc,acceptParentFunc);
        // assert(callgraphBegin != NULL);
        // int StateMachineCGLevel = findStateMachineCallGraphLevel(MergeNodeVec_level,callgraphBegin);
        errs()<<"StateMachineCGLevel: "<<StateMachineCGLevel<<"\n";
        //以此函数为起始点开始向下遍历:
        /*
            黑名单（不记录以下函数）：
                1.Function的basicblock数目较少的，意味着这种函数可能没啥意义，不应该记录
                2.如果是accept()相关回溯上来的函数，也不再考虑；
                3.对于状态机层面的关键变量，遍历层数不应该取很多；待统计
            向下遍历层数有两类情景：1.找状态机层面的 2.找普通类型的
                1.找状态机层面的:范围需小，不宜固定死，考虑->取交叉函数结点与read()之间相差的最大层数作为此层的范围
                2.普通类型的，范围给大一点没事
        */
        errs()<<"CallGraph的起始点： "<<callgraphBegin->getName()<<"\n";

        if (giveBeginFunction != NULL){
            callgraphBegin = M.getFunction(StringRef(giveBeginFunction));
        }

        iterateCallSiteFromTargetFunc(callgraphBegin,CallGraphMap_State,StateMachineCGLevel);
        iterateCallSiteFromTargetFunc(callgraphBegin,CallGraphMap_All,StateMachineCGLevel*1.6);
        errs()<<"CallGraphMap_State size: "<<CallGraphMap_State.size()<<"\n";
        errs()<<"CallGraphMap_All size: "<<CallGraphMap_All.size()<<"\n";
        for (auto one : CallGraphMap_All)
        {
            errs()<<one.first->getName()<<"\t"<<one.second<<"\n";
        }
    }else{
        int StateMachineCGLevel = 5;
        Function *callgraphBegin = M.getFunction(StringRef(giveBeginFunction));

        // iterateCallSiteFromTargetFunc(callgraphBegin,CallGraphMap_State,StateMachineCGLevel);
        iterateCallSiteFromTargetFunc(callgraphBegin,CallGraphMap_All,StateMachineCGLevel*2);
        errs()<<"CallGraphMap_State size: "<<CallGraphMap_State.size()<<"\n";
        errs()<<"CallGraphMap_All size: "<<CallGraphMap_All.size()<<"\n";
        for (auto one : CallGraphMap_All)
        {
            errs()<<one.first->getName()<<"\t"<<one.second<<"\n";
        }
    }
}
/**
 * @description: 从Main向下遍历5层，选择最深的交叉函数点并返回
 * @param {Module} &M
 * @param {vector<Function*>} MergeNodeVec
 * @return {*}
 */
Function* chooseTheMostDeepFunction(Module &M,vector<Function*> MergeNodeVec,vector<Function*> recvParentFunc,vector<Function*> acceptParentFunc){
    map<Function*,int> resultsMap;
    Function *MainFunction = M.getFunction(StringRef("main"));
    //向下遍历五层，获取函数的深度信息
    iterateCallSiteFromTargetFunc(MainFunction,resultsMap,5);

    vector<pair<Function*,int>> resVec;
    for (auto one : MergeNodeVec)
    {
        map<Function*,int>::iterator it = resultsMap.find(one);
        if (it != resultsMap.end()){
            resVec.push_back(pair<Function*,int>(it->first,it->second));
        }
    }
    std::sort(resVec.begin(),resVec.end(),judge);
    //不能单纯选最深的,而应像邻接表那样来:从深度深的开始遍历，如果有结点调用了多个结点，则用它；如果没有，则直接选最深的。
    Function *retFunc = createGraphToChoose(resVec,recvParentFunc,acceptParentFunc);
    return retFunc;
    /*如果有下列的情况:选func3，而不是最深的
      fun1
        \
         fun3->func4->main              fun3->func4->main 选func4
        /
      fun2
    */
}

Function* createGraphToChoose(vector<pair<Function*,int>> resVec,vector<Function*> readVec,vector<Function*> acceptVec){
    int length = resVec.size();
    VertexNode AdjList[length];
    //初始顶点表节点
    for (int i=0;i<resVec.size();i++)
    {
        AdjList[i].function = resVec[i].first;
        AdjList[i].level = resVec[i].second;
        AdjList[i].edgeCount = 0;
        AdjList[i].firstedge = NULL;
    }
    //构建其他的边表节点
    for (size_t i = 0; i < resVec.size(); i++)
    {
        for (size_t j = i+1; j < resVec.size(); j++)
        {
            Function *A = resVec[i].first;       
            Function *B = resVec[j].first;
            if (checkIfAwasCalledB(A,B))
            {
                int pos_B;
                //找函数B的在顶点表的位置
                for (pos_B = 0; pos_B < length && AdjList[pos_B].function != B; pos_B++);
                //在B所在顶点表后续插入A
                EdgeNode *s = (EdgeNode*)malloc(sizeof(EdgeNode));
                s->function = A;
                s->next = NULL;
                //如果B后没有结点->直接附加
                if (AdjList[pos_B].firstedge==NULL)
                {
                    AdjList[pos_B].firstedge = s;
                    AdjList[pos_B].edgeCount++;
                }else{
                    EdgeNode *q = AdjList[pos_B].firstedge;
                    while(q->next != NULL){
                        q = q->next;
                    }
                    q->next = s;
                    AdjList[pos_B].edgeCount++;
                }
            }
        }
    }
    //"打印功能"
    for (int i = 0;i < resVec.size();i++)
    {
        errs()<<"顶点"<<i<<": Function: "<<AdjList[i].function->getName()<<"\tlevel: "<<AdjList[i].level<<"\tedgeCount: "<<AdjList[i].edgeCount<<"\n";
    }
    // 选择合适的点作为交叉函数结点
    //找到交叉点最Max的那个点，从那儿开始回溯折半
    int maxedgeCount=0,maxedgePos=0;
    for (size_t i = 0; i < resVec.size(); i++)
    {
        if (AdjList[i].edgeCount > maxedgeCount)
        {
            maxedgeCount = AdjList[i].edgeCount;
            maxedgePos = i;
        }
    }
    //折半再次选择：fun3->func4->main 选中间的那个
    maxedgePos += (length - maxedgePos - 1)/2;
    

    //避免出现错误匹配，将accept函数与read的父函数作为交叉函数结点，当然有可能，所以也开了个口子
    // vector<Function*> readAndAcceptVec;
    // readAndAcceptVec.insert(readAndAcceptVec.end(),readVec.begin(),readVec.end());
    // readAndAcceptVec.insert(readAndAcceptVec.end(),acceptVec.begin(),acceptVec.end());
    // while (find(readAndAcceptVec.begin(),readAndAcceptVec.end(),AdjList[maxedgePos].function)!=readAndAcceptVec.end())
    // {
    //     maxedgePos++;
    // }
    // if (maxedgePos>=length)
    // {
    //     maxedgeCount = length-1;
    // }
    errs()<<"选择的交叉函数结点是:"<<maxedgePos<<"\tFunction: "<<AdjList[maxedgePos].function->getName()<<"\n";
    //释放内存
    for (size_t i = 0; i < resVec.size(); i++)
    {
        if (AdjList[i].edgeCount != 0)
        {
            EdgeNode *p = AdjList[i].firstedge;
            while (p->next != NULL)
            {
                EdgeNode *q = p->next;
                free(p);
                p = q;
            }
            free(p);
        }
    }
    return AdjList[maxedgePos].function;
}

bool judge(const pair<Function*,int> a, const pair<Function* ,int> b) {
    return a.second > b.second;
}

bool judge_re(const pair<Function*,int> a, const pair<Function* ,int> b) {
    return a.second < b.second;
}

/**
 * @description: vector去重（影响顺序）
 * @param {*}
 * @return {*}
 */
template <typename T>
void uniqueVector(vector<T> &targetVec){
    set<T> MergeSet(targetVec.begin(),targetVec.end());
    targetVec.assign(MergeSet.begin(),MergeSet.end());
}
/**
 * @description: 选取状态机关键变量的callgraph匹配层数
 * @param {vector<Function,int>} resultVec
 * @return {*}
 */
int findStateMachineCallGraphLevel(vector<pair<Function*,int>> resultVec,Function *callgraphBegin){
    //选择resultVec中callgraphBegin的一系列，选其中最大的int作为匹配层数（int：read与callgraphBegin之间的距离）
    vector<int> candidateValue;
    for (auto one : resultVec)
    {
        if (one.first == callgraphBegin)
        {
            candidateValue.push_back(one.second);
        }
    }
    auto maxValue = max_element(candidateValue.begin(), candidateValue.end());
    return *maxValue;
    // std::sort(resultVec.begin(),resultVec.end(),judge);
    // assert(resultVec.size()!=0);
    // return resultVec[0].second;
}
/**
 * @description: 提取全局变量型函数指针的初始化情况
 * @param {GlobalVariable} *FuncGV 目标
 * @param {vector<Function*>} &functionVec 保存结果
 * @return {*}
 */
void extractGVFuncPointerInitialInfo(Module *M,GlobalVariable *FuncGV,vector<Function*> &functionVec){
    for (auto &function : M->getFunctionList())
    {   
        
        for (inst_iterator inst_iter = inst_begin(function);inst_iter != inst_end(function); inst_iter++)
        {
            Instruction *instruction = cast<Instruction>(&*inst_iter);
            //暂只支持Store指令
            if (StoreInst *storeinst = dyn_cast<StoreInst>(instruction))
            {
                if (Function *func = dyn_cast<Function>(storeinst->getOperand(0)))
                {
                    // errs()<<"func->isDeclaration(): "<< func->isDeclaration() <<"\n";
                    if (func->isDeclaration()==0)
                    {
                        // errs()<<"保存:"<<func->getName()<<"\n";
                        functionVec.push_back(func);
                    }
                }                                
            }
        }
    }
}
/**
 * @description: 计算指定函数与Main函数之间的层数（确保targetFunc是回溯的开始，otherfunc是向上回溯的终点）
 * @param {Module} &M
 * @param {Function} *targetFunc:回溯的起始函数
 * @param {Function} *otherFunc:回溯终点函数
 * @return {*}
 */
int calLevelBetweenTargeFuncAndOthers(Module &M,Function *targetFunc,Function *otherFunc){
    if (targetFunc == otherFunc)
    {
        return 0;
    }

    int maxBackTraceLevel = 8;

    queue<vector<Function*>> ffqueue;
    vector<Function*> ParentFunc;
    ParentFunc.push_back(targetFunc);
    ffqueue.push(ParentFunc);

    int ccurPos = 0;
    while (ccurPos < maxBackTraceLevel && !ffqueue.empty())
    {
        vector<Function*> tmp = ffqueue.front();

        ffqueue.pop();
        ccurPos++;
        if (backtraceForCalLevel(tmp,ffqueue,otherFunc)){
            return ccurPos;
        }
    }
    return ccurPos;
}

bool backtraceForCalLevel(vector<Function*> functionVec,queue<vector<Function*>> &fqueue,Function *targetFunc){
    vector<Function*> parentFuncVec;
    map<Function*,int> parentFuncVec_tmpmap;//辅助parentFuncVec，避免重复插入

    //iterate the beginning og the backtrace function
    for (auto function : functionVec)
    {
        //check which instructions use these function
        for (Value::use_iterator ui = function->use_begin(); ui != function->use_end(); ui++)
        {
            if (Instruction *call = dyn_cast<Instruction>(ui->getUser()))
            {
                CallSite callsite(call);
                if (callsite)
                {
                    // 显示调用了该函数
                    Function *called = callsite->getFunction();
                    if (called == targetFunc)
                    {
                        return true;
                    }
                    if (parentFuncVec_tmpmap.find(called) == parentFuncVec_tmpmap.end())
                    {
                        parentFuncVec_tmpmap.insert(pair<Function*,int>(called,0));
                        parentFuncVec.push_back(called);
                    }
                }else if (StoreInst *storeinst = dyn_cast<StoreInst>(call))
                {
                    /*
                        proftpd cmd_loop案例
                    */
                    if (isa<Function>(storeinst->getOperand(0)))
                    {
                        GlobalVariable *gv = dyn_cast<GlobalVariable>(storeinst->getOperand(1));
                        if (gv)
                        {
                            vector<Function*> resVe;
                            checkGlobalVariablesIfCall(gv,resVe);
                            if (resVe.size()>0)
                            {
                                //TODO:需要计算resVe中的函数与targetFunc之间的差是多少，返回给参数出去
                                //因为如果直接return true，仍然认为在当前函数中调用了它，但实际上是其他的函数做了调用这个行为，需要做！
                                //TODO:感觉没太大必要加，先不加吧
                                return true;
                            }
                        }
                    }
                }
            }else{
                // 该函数被用于某一个函数的参数中，如proftpd中由pthread_create调用ftp_client_thread()
                User *user = ui->getUser();
                for (auto usr : user->users())
                {
                    if (CallInst *call = dyn_cast<CallInst>(usr))
                    {
                        if (call->getFunction() == targetFunc)
                        {
                            return true;
                        }
                        if (parentFuncVec_tmpmap.find(call->getFunction()) == parentFuncVec_tmpmap.end())
                        {
                            parentFuncVec_tmpmap.insert(pair<Function*,int>(call->getFunction(),0));
                            parentFuncVec.push_back(call->getFunction());
                        } 
                    }
                }
            }
        }
    }
    if (parentFuncVec.size()!=0)
    {
        fqueue.push(parentFuncVec);
    }
    return false;
}
/**
 * @description: check if A was called by B（A是否被B调用，即B调用A）
 * @param {Function} *A
 * @param {Function} *B
 * @return {*}
 */
bool checkIfAwasCalledB(Function *A,Function *B){
    for (Value::use_iterator ui = A->use_begin(); ui != A->use_end(); ui++)
    {
        if (Instruction *call = dyn_cast<Instruction>(ui->getUser()))
        {
            CallSite callsite(call);
            if (callsite)
            {
                // 显示调用了该函数
                Function *called = callsite->getFunction();
                if (called == B)
                {
                    return true;
                }             
            }
        }else{
            // 该函数被用于某一个函数的参数中，如proftpd中由pthread_create调用ftp_client_thread()
            User *user = ui->getUser();
            for (auto usr : user->users())
            {
                if (CallInst *call = dyn_cast<CallInst>(usr))
                {
                    if (call->getFunction() == B)
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
 * @description: 检查被call的这个指令是否指向全局数组
 * @param {Instruction} *instruction
 * @return {*}
 */
GlobalVariable* checkCalledObjIfPointToGlobalArray(Instruction *instruction){
    /*
        %arrayidx94 = getelementptr inbounds [32 x %struct._FTPROUTINE_ENTRY], [32 x %struct._FTPROUTINE_ENTRY]* @ftpprocs, i64 0, i64 %idxprom93, !dbg !4836
        %Proc = getelementptr inbounds %struct._FTPROUTINE_ENTRY, %struct._FTPROUTINE_ENTRY* %arrayidx94, i32 0, i32 1, !dbg !4837
        %52 = load i32 (%struct._FTPCONTEXT*, i8*)*, i32 (%struct._FTPCONTEXT*, i8*)** %Proc, align 8, !dbg !4837
        %call95 = call i32 %52(%struct._FTPCONTEXT* %ctx, i8* %53), !dbg !4836
    */
    if (LoadInst *loadinst = dyn_cast<LoadInst>(instruction))
    {
        Value *loadinstVal = loadinst->getOperand(0);
        if (GetElementPtrInst *struct_func = dyn_cast<GetElementPtrInst>(loadinstVal))
        {
            Instruction *Gep2 = dyn_cast<Instruction>(struct_func->getOperand(0));
            if (Gep2)
            {
                Value *Gep2_op = Gep2->getOperand(0);
                if (GlobalVariable *globalArray = dyn_cast<GlobalVariable>(Gep2_op))
                {
                    return globalArray;
                }
            }
        }
    }
    return NULL;
}

void iterateGlobalArrayToStoreFunction(GlobalVariable *globalArray,vector<Function*> &storedVec){
    if (globalArray->hasInitializer() == false)
    {
        return;
    }
    
    Use *operand = globalArray->getInitializer()->getOperandList();
    for (int i = 0; i < globalArray->getInitializer()->getNumOperands(); i++)
    {
        Function *func = dyn_cast<Function>(operand[i].get());
        if (func != NULL && !func->isDeclaration())
        {
            // if element is user defined function
            storedVec.push_back(func);
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
                        storedVec.push_back(call);
                    }
                }
            }
        }
    }
}

bool checkIfTCPorUDP(){
    char *protocol_type_env;
    protocol_type_env = getenv("PROTOCOL_TYPE");
    if (protocol_type_env!=NULL && strcmp(protocol_type_env,"UDP")==0)
    {
        return true;
    }
    return false;
}

bool checkIfBlackList(StringRef funcname){
    // blacklist like : read configure file、
    if (funcname.contains(StringRef("read"))&&funcname.contains(StringRef("conf")))
    {
        return true;
    }
    // config_xxx ...
    if (funcname.startswith_lower(StringRef("config"))){
        return true;
    }
    return false;
}

// void getBlackFunctionList(){
//     vector<string> blackListVec;
//     char *black_function_path = getenv("BLACK_FUNCTION_PATH");
//     if (black_function_path != NULL)
//     {
//         ifstream fInstream(black_function_path);
//         if (fInstream)
//         {
//             string str;
//             while(getline(fInstream,str)){
//                 errs()<< str <<"\n";
//             }
//         }else{
//             errs()<<"[open fail]\n";
//         }
//         fInstream.close();
//     }
// }