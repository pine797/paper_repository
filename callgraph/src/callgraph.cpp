#include "../include/cppheader.h"
#include "../include/llvmheader.h"
#include "../include/callgraph.h"

using namespace std;
using namespace llvm;

void readRecvFuncStackInfo(string filepath,vector<FunctionStackInfo> &FuncStackVec){
    ifstream readFile;
    readFile.open(filepath,ios::in);
    
    vector<string> tmpVec;  
    
    if (readFile.is_open())
    {
        string str;
        while (getline(readFile,str)){
            tmpVec.push_back(str);
        }
    }
    readFile.close();

    int length = tmpVec.size();
    vector<int> tmpVec2;
    map<int,int> stackRange;    
    for (int i = 0; i < tmpVec.size(); i++){
        string str = tmpVec[i];
        if (i == 0){
            tmpVec2.push_back(i);
            
        }
        else if (i == length - 1){
            tmpVec2.push_back(i-1);
            
        }
        else if (i != length - 1 && str.rfind("[==========]",0) != string::npos){
            tmpVec2.push_back(i-1);
            
            
            tmpVec2.push_back(i+1);
        }
    }
    for (size_t i = 0,j=i+1; j < tmpVec2.size(); i+=2,j+=2)
    {
        errs()<<tmpVec2[i]<<" ~ "<<tmpVec2[j] << "\n";
        stackRange.insert(pair<int,int>(tmpVec2[i],tmpVec2[j]));
    }
    
    
    for (auto iter = stackRange.begin();iter != stackRange.end(); iter++)
    {
        int first = iter->first, second = iter->second;
        FunctionStackInfo funstackinfo;
        for (size_t i=first,j=i+1; i <= second; i++,j++)
        {
            funstackinfo.funcStackVec.push_back(tmpVec[i]); 
        }
        for (size_t i=first,j=i+1; j <= second; i++,j++){
            funstackinfo.funcStackMap.insert(pair<string,string>(tmpVec[i],tmpVec[j])); 
        }
        FuncStackVec.push_back(funstackinfo);
    }
}

Function* checkIfStackInfoContainsThread(Module &M,vector<FunctionStackInfo> &FuncStackVec){
    for (size_t i = 0; i < FuncStackVec.size(); i++)
    {
        FunctionStackInfo funcStackInfo = FuncStackVec[i];
        for (int j = 0; j <= funcStackInfo.funcStackVec.size()-1 ; j++)
        {
            
            // errs()<<"funcStackInfo funcStackVec size "<<funcStackInfo.funcStackVec.size()<<"\n";
            
            string funcname = funcStackInfo.funcStackVec[j];
            if (funcname.rfind("clone",0) != string::npos || funcname.rfind("start_thread",0) != string::npos)
            {
                
                for (int pos = j+1; pos <=funcStackInfo.funcStackVec.size()-1; pos++)
                {
                    string curFuncName = funcStackInfo.funcStackVec[pos];
                    errs()<<"curFuncName: "<<curFuncName<<"\tfuncname: "<<funcname<<"\n";
                    Function *function = M.getFunction(StringRef(curFuncName));
                    if (function){
                        return function;
                    }else{
                        continue;
                    }
                }
            }   
        }
    }
    
    
    return NULL;
}
void test(Module *M){
    for (auto &function : M->getFunctionList())
    {   
        errs()<<function.getName()<<"\t"<<function.isDeclaration()<<"\n";
    }
}

Function* findStartFunction(Module *M,vector<FunctionStackInfo> &FuncStackVec,int &SkipFlags){
    Function *EntryFunction = checkIfStackInfoContainsThread(*M,FuncStackVec);
    vector<Function*> ReadFunctionVec;

    if (EntryFunction){
        SkipFlags = 1;
        return EntryFunction;
    }

    
    for (auto &function : M->getFunctionList())
    {   
        Function *curFunction = &function;
        if (curFunction->isDeclaration())   
        {
            continue;
        }

        for (inst_iterator inst_iter = inst_begin(curFunction);inst_iter != inst_end(curFunction); inst_iter++)
        {
            Instruction *instruction = cast<Instruction>(&*inst_iter);
            CallSite callsite(instruction);

            if (callsite){
                if (isa<Function>(callsite.getCalledValue())) {
                    Function *readFunc = callsite.getCalledFunction();

                    if (readFunc != NULL && readFunc->isDeclaration()){ 
                        bool flags_select = readFunc->getName().equals(StringRef("select"));
                        bool flags_poll = readFunc->getName().equals(StringRef("poll"));
                        bool flags_epoll = readFunc->getName().equals(StringRef("epoll_wait"));
                        if (flags_select | flags_poll | flags_epoll){
                            ReadFunctionVec.push_back(curFunction); 
                            if (checkEntryFunction(curFunction,FuncStackVec)) {
                                return curFunction;
                            }
                        }
                    }
                }
                continue;
            }
        }
    }
    for (auto &function : M->getFunctionList())
    {
        Function *curFunction = &function;
        if (curFunction->isDeclaration())   
        {
            continue;
        }
        
        
        for (inst_iterator inst_iter = inst_begin(function);inst_iter != inst_end(function); inst_iter++)
        {
            Instruction *instruction = cast<Instruction>(&*inst_iter);
            CallSite callsite(instruction);
            if (callsite)
            {
                if (callsite.getCalledFunction() == NULL || callsite.getCalledFunction()->getName().startswith(StringRef("llvm."))){
                    continue;
                }
                // [situation1 : called function has casted] , e.g. call void (...) bitcast (void ()* @expand_groups to void (...)*)()
                if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction->getOperand(0)))
                {
                    if (cstexpr->isCast())
                    {
                        Function *func = dyn_cast<Function>(cstexpr->stripPointerCasts());
                        if (func && !func->isDeclaration() && is_element_in_function_vector(ReadFunctionVec,func))   
                        {
                            
                            if (checkEntryFunction(curFunction,FuncStackVec)) { 
                                
                                return curFunction;
                            }
                        }
                    }
                }
                else
                {
                    if (isa<Function>(callsite.getCalledValue()))
                    {
                        
                        // [situation3 : call function], e.g. call void (i8, i8*, ...) @control_printf(...)
                        if ( Function *func = dyn_cast<Function>(callsite.getCalledValue()))
                        {
                            if (func->isDeclaration()==0 && is_element_in_function_vector(ReadFunctionVec,func))  
                            {
                                if (checkEntryFunction(curFunction,FuncStackVec)) {
                                    
                                    return curFunction;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    

    assert(EntryFunction!=NULL);
    return NULL;
}

bool checkEntryFunction(Function *curFunction,vector<FunctionStackInfo> &FuncStackVec){
    string funcName = curFunction->getSubprogram()->getName().str();    
    
    for (int i=0;i<FuncStackVec.size();i++){
        FunctionStackInfo functionStackInfo = FuncStackVec[i];
        
        for (int j=0;j<functionStackInfo.funcStackVec.size();j++){
            string stackFuncName = functionStackInfo.funcStackVec[j];
            
            
            
            if (funcName == stackFuncName || funcName.find(stackFuncName) != string::npos){
                return true;  
            }
        }
    }
    return false;
}

void extractCallGraph(Module &M,map<Function*,int> &CallGraphMap,vector<FunctionStackInfo> funcStackInfo,Function *EntryFunc,int CallLevel,int SkipBeforeCode){

    iterateCallSiteFromTargetFunc(M,EntryFunc,CallGraphMap,funcStackInfo,CallLevel,SkipBeforeCode);

    errs()<<"CallGraphMap_All size: "<<CallGraphMap.size()<<"\n";
    for (auto one : CallGraphMap)
    {
        errs()<<one.first->getSubprogram()->getName()<<"\t"<<one.second<<"\t"<<one.first->getSubprogram()->getFilename()<<"\n";
    } 
}

void iterateCallSiteFromTargetFunc(Module &M,Function *SourceFunc,map<Function*,int> &resultsMap,vector<FunctionStackInfo> funcStackInfo,int maxLevel,int skipBeforeEvent){
    int level = 1;
    queue<vector<Function*>> fqueue;
    vector<Function*> ChildFunc;
    ChildFunc.push_back(SourceFunc);
    fqueue.push(ChildFunc);
    resultsMap.insert(pair<Function*,int>(SourceFunc,level));

    while (level <= maxLevel && !fqueue.empty())
    {
        vector<Function*> tmp = fqueue.front();
        fqueue.pop();

        
        if (level == 1 && skipBeforeEvent == 1){
            iterateOneLevel(M,tmp,resultsMap,funcStackInfo,level,fqueue,1);
        }else {
            iterateOneLevel(M,tmp,resultsMap,funcStackInfo,level,fqueue,0);
        }
        level++;
    }
}


void iterateOneLevel(Module &M,vector<Function*> beginVec,map<Function*,int> &resultsMap,vector<FunctionStackInfo> funcStackInfo,int level,queue<vector<Function*>> &fqueue, int skipBeforeEvent){
    vector<Function*> tmpresults;   
    bool readyExtractFlags = false;

    for (auto func : beginVec)
    {   
        if (checkIfBlackList(func)){
            continue;
        }
        if (func->isDeclaration() || (checkFunctionIfContainCallInst(func)==false && func->getBasicBlockList().size()<5) ) 
        {
            continue;
        }

        
        for (inst_iterator Inst = inst_begin(func),End = inst_end(func);Inst != End; ++Inst)
        {
            Instruction *instruction = cast<Instruction>(&*Inst);
            CallSite callsite(instruction);

            if (skipBeforeEvent == 1 && readyExtractFlags == false){
                if (callsite) {
                    Function *readFunc = callsite.getCalledFunction();
                    
                    if (readFunc != NULL && readFunc->isDeclaration()){
                        bool flags_select = readFunc->getName().equals(StringRef("select"));
                        bool flags_poll = readFunc->getName().equals(StringRef("poll"));
                        bool flags_epoll = readFunc->getName().equals(StringRef("epoll_wait"));
                        if (flags_select | flags_poll | flags_epoll){
                            readyExtractFlags = true;
                            continue;
                        }
                    }
                }
            }

            if (callsite)
            {
                // [situation1 : called function has casted] , e.g. call void (...) bitcast (void ()* @expand_groups to void (...)*)()
                if (auto *cstexpr = dyn_cast<ConstantExpr>(instruction->getOperand(0)))
                {
                    if (cstexpr->isCast())
                    {
                        Function *function = dyn_cast<Function>(cstexpr->stripPointerCasts());
                        if (function && function->isDeclaration()==0 && !checkIfBlackList(function))
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
                    if (isa<Instruction>(callsite.getCalledValue())) 
                    {
                        int funcPos = instruction->getNumOperands()-1;
                        Instruction *loadInst = dyn_cast<Instruction>(instruction->getOperand(funcPos));
                        if (loadInst && isa<GlobalVariable>(loadInst->getOperand(0)))
                        {
                            GlobalVariable *FuncGV = cast<GlobalVariable>(loadInst->getOperand(0));
                            
                            vector<Function*> functionVec;
                            extractGVFuncPointerInitialInfo(loadInst->getModule(),FuncGV,functionVec);
                            for (auto func_tmp:functionVec)
                            {
                                
                                if (!checkIfBlackList(func_tmp)){
                                    tmpresults.push_back(func_tmp);
                                    resultsMap.insert(pair<Function*,int>(func_tmp,level));
                                }
                            }
                        }else if(GlobalVariable *globalArray = checkCalledObjIfPointToGlobalArray(loadInst)){
                            
                            
                            vector<Function*> storedVec;
                            iterateGlobalArrayToStoreFunction(globalArray,storedVec);
                            for (auto func_tmp:storedVec)
                            {
                                if (!checkIfBlackList(func_tmp)){
                                    tmpresults.push_back(func_tmp);
                                    resultsMap.insert(pair<Function*,int>(func_tmp,level));
                                }
                            }
                        }
                    }
                    else if (isa<Function>(callsite.getCalledValue()))
                    {
                        
                        // [situation3 : call function], e.g. call void (i8, i8*, ...) @control_printf(...)
                        if ( Function *function = dyn_cast<Function>(callsite.getCalledValue()))
                        {
                            if (function->isDeclaration()==0)
                            {
                                if (!checkIfBlackList(function)){
                                    resultsMap.insert(pair<Function*,int>(function,level));
                                    tmpresults.push_back(function);
                                }
                            }
                        }
                        // resultsMap.insert(pair<Function*,int>(callsite.getCalledFunction(),level));
                        // tmpresults.push_back(callsite.getCalledFunction());
                        
                        Use *args = callsite.getInstruction()->getOperandList();
                        for (size_t i = 0; i < callsite.getInstruction()->getNumOperands()-1; i++)
                        {
                            
                            if (Function *callfunc = dyn_cast<Function>(args[i].get()))
                            {
                                if (callfunc->isDeclaration()==0 && !checkIfBlackList(callfunc))
                                {
                                    resultsMap.insert(pair<Function*,int>(callfunc,level));
                                    tmpresults.push_back(callfunc);
                                }
                            }else if (auto *cstexpr = dyn_cast<ConstantExpr>(args[i].get()))
                            {
                                
                                Function *function = dyn_cast<Function>(cstexpr->stripPointerCasts());
                                if (function && function->isDeclaration()==0 && !checkIfBlackList(function))
                                {
                                    resultsMap.insert(pair<Function*,int>(function,level));
                                    tmpresults.push_back(function);
                                }
                            }
                        }
                    }
                }
            }
            
            if (GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(instruction)){
                Instruction *gepObj = dyn_cast<Instruction>(gepInst->getOperand(0));
                if (gepObj){
                    Value *obj = gepObj->getOperand(0);
                    if (GlobalVariable *globalArray = dyn_cast<GlobalVariable>(obj))
                    {
                        
                        vector<Function*> storedVec;
                        iterateGlobalArrayToStoreFunction(globalArray,storedVec);
                        for (auto func_tmp:storedVec)
                        {
                            if (!checkIfBlackList(func_tmp)){
                                tmpresults.push_back(func_tmp);
                                resultsMap.insert(pair<Function*,int>(func_tmp,level));
                            }
                        }
                    }
                }
            }   
        }
        
        string funcname = func->getFunction().getSubprogram()->getName();
        for (size_t i = 0; i < funcStackInfo.size(); i++)
        {
            map<string,string> funcStackMap = funcStackInfo[i].funcStackMap;
            if (funcStackMap.find(funcname) != funcStackMap.end()){
                string callerFunc = funcStackMap[funcname];
                Function *callerFunction = M.getFunction(StringRef(callerFunc));
                if (callerFunction && checkIfBlackList(callerFunction)){
                    resultsMap.insert(pair<Function*,int>(callerFunction,level));
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

bool checkIfBlackList(Function *function){
    string funcname;
    if (function->getSubprogram())
    {
        funcname = function->getSubprogram()->getName().lower();
        if (function->getSubprogram()->getFilename().endswith(StringRef(".h")))
        {
            return true;
        }
    }else{
        funcname = function->getName().lower();
    }
    transform(funcname.begin(),funcname.begin(),funcname.end(),::toLower);
    
    if (function->isDeclaration())
    {
        return true;
    }        
    
    if (funcname.rfind("read",0) != string::npos && funcname.rfind("conf",0) != string::npos)
    {
        return true;
    }
    // config_xxx ...
    if (funcname.rfind("config",0) == 0 || funcname.rfind("operator",0) == 0 || funcname.rfind("~",0) == 0 || funcname.empty())
    {
        return true;
    }

    return false;
}

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
void extractGVFuncPointerInitialInfo(Module *M,GlobalVariable *FuncGV,vector<Function*> &functionVec){
    for (auto &function : M->getFunctionList())
    {   
        for (inst_iterator inst_iter = inst_begin(function);inst_iter != inst_end(function); inst_iter++)
        {
            Instruction *instruction = cast<Instruction>(&*inst_iter);
            
            if (StoreInst *storeinst = dyn_cast<StoreInst>(instruction))
            {
                if (Function *func = dyn_cast<Function>(storeinst->getOperand(0)))
                {
                    
                    if (func->isDeclaration()==0)
                    {
                        
                        functionVec.push_back(func);
                    }
                }                                
            }
        }
    }
}
GlobalVariable* checkCalledObjIfPointToGlobalArray(Instruction *instruction){
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
            
            storedVec.push_back(func);
        }
        else
        {
            
            Constant *gv = dyn_cast<Constant>(operand[i].get());
            if (gv != NULL && gv->getType()->isStructTy())
            {
                
                for (Use &U : gv->operands())
                {
                    Function *call = dyn_cast<Function>(U.get());
                    
                    if (call != NULL && !call->isDeclaration())
                    {
                        storedVec.push_back(call);
                    }
                }
            }
        }
    }
}

bool checkIfContaintsTwoList(string funcName,vector<string> WhiteList1,vector<string> WhiteList2){
    
    transform(funcName.begin(), funcName.end(), funcName.begin(), ::tolower);
    for (auto iter1 : WhiteList1)
    {
        string str1 = iter1;
        if (funcName.find(str1) != string::npos)
        {
            
            for (auto iter2 : WhiteList2)
            {
                string str2 = iter2;
                
                if (funcName.find(str2) != string::npos)
                {
                    
                    return true;
                }
            }
        }
    }
    return false;
}

void extractOtherCallGraph(Module &M,map<Function*,int> &CallGraphMap){
    vector<string> WhiteList1 = {"handle","process","read","write","parse","dispatch","receive"};
    vector<string> WhiteList2 = {"packet","request","client","req","state","machine","server","command","cmd","message","msg","statemahcine","data"};


    for (auto &function:M.getFunctionList())
    {
        
        //     errs()<<function.getSubprogram()->getName()<<"\t"<<function.getName()<<"\n";
        
        if (checkIfBlackList(&function)){
            continue;
        }
        if (function.isDeclaration() || function.getBasicBlockList().size()<5 )
        {
            continue;
        }
        
        if (function.getSubprogram()->getName().startswith(StringRef("~")) || function.getSubprogram()->getName().startswith(StringRef("operator")))
        {
            continue;
        }
        
        if (CallGraphMap.find(&function) != CallGraphMap.end()) {   
            continue;
        }
        string funcname = function.getSubprogram()->getName().lower();
        
        if (checkIfContaintsTwoList(funcname,WhiteList1,WhiteList2)){
            CallGraphMap.insert(pair<Function*,int>(&function,88));
        }
    }

    errs()<<"CallGraphMap(Other) size: "<<CallGraphMap.size()<<"\n";
    for (auto one : CallGraphMap)
    {
        if (one.second == 88){
            errs()<<one.first->getSubprogram()->getName()<<"\t"<<one.second<<"\t"<<one.first->getSubprogram()->getFilename()<<"\n";
        }
    }
}

bool is_element_in_function_vector(vector<Function*> v,Function* element){
    vector<Function*>::iterator it;
    it=find(v.begin(),v.end(),element);
    if (it!=v.end()){
        return true;
    }
    else{
        return false;
    }
}