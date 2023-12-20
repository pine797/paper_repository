#include "globalvar.h"
#include "funcpointer.h"
#include <fstream>
#include <iomanip>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/raw_ostream.h>
#include <unistd.h>
#include <stdio.h>
#include <algorithm>
#include <regex>
#include <sstream>


using namespace std;
using namespace llvm;


// 记录已经发现组合型和单一型关键变量的个数
int curStateMachineNum = 8096;
int curSubStateNum = 0;

//存储已经插桩监控的instruct，避免在同一基本块内重复监控
vector<Value*> AlreadyMonitorVarVec;

//记录所有插桩信息
vector<InstrumentInfo*> AllInstrumentInfoVec;

//标识项目语言
int PROGRAM_LANG_FLAGS; //0:c,1:cpp

//Word2vec模型文件描述符
Word2VecData word2vecdata;
bool IfEnableWord2vec=false;
extern map<string,float*> stateDictMap;
extern bool IfEnableWord2vec;

//维护状态机变量识别所需要的一些列信息
StateMachineVarAssistInfo stateMachineAssistInfo;
// the file ranges for analyzing statemachine variable
vector<string> stateMachineCallGraph;


void extractInstInfoForMealyVar(Instruction *instruction,string varName,int type){
    // dump candiate mealyvar info to file
    string outfilename = "./candidate_mealyvar.txt";
    ofstream outfile;
    outfile.open(outfilename,ios::app);
    if (!outfile.is_open()){
        errs()<<"open file failed\n";
        return;
    }
    
    string sceneType;
    switch (type)
    {
        case 0:
            sceneType = "switch";
            break;
        case 1:
            sceneType = "if";
            break;
        case 2:
            sceneType = "indirect";
            break;
    }
    // errs()<<"instruction: "<<*instruction<<"\n";
    string filename = instruction->getDebugLoc()->getDirectory().str() + "/" + instruction->getDebugLoc()->getFilename().str();
    int line = instruction->getDebugLoc()->getLine();
    string location = filename + ":" + to_string(line);
    string breakpoint = instruction->getDebugLoc()->getFilename().str() + ":" + to_string(line);
    string funcname = instruction->getFunction()->getSubprogram()->getName();
    outfile<< "SceneType: "<<sceneType<<"\tVarName: "<<varName<<"\tLocation: "<<location<<"\tFuncname: "<<funcname<<" [b " << breakpoint << "]\n";

    

    outfile.close();
}

void loadStateVariableDictFile(vector<string> &stateDicVec){
    string filename = "./stateVariableDict.txt";
    // check file exist //TODO:

    ifstream infile;
    infile.open(filename,ios::in);
    if (!infile.is_open()){
        errs()<<"open stateVariableDict file failed! No dictianary now. \n";
        return;
    }
    string line;
    while (getline(infile,line))
    {
        if (line.empty())
        {
            continue;
        }
        stateDicVec.push_back(line);
    }
    infile.close();

    errs() << "[*] load stateVariableDict file success, size: " << stateDicVec.size() << "\n";
    // for (auto one:stateDicVec)
    // {
    //     errs() << one << "\t"<<one.size()<<"\n";
    // }
}


// 适用于函数指针模块、其他类型检测模块插桩使用
void InsertFunction(Module *M,Instruction *locateInst,Value *targetInst,int sceneType,int stateType,ArrayRef<Value*> funcArgs,int &error){

    if (!locateInst->getNextNode())
    {
        return;
    }
    // errs()<<"end with .h "<<locateInst->getFunction()->getSubprogram()->getFilename().endswith(".h")<<"\n";
    // if (locateInst->getFunction()->getSubprogram()->getFilename().endswith(".h") == 1){
    //     errs()<<".h文件不插桩\n";
    // }
    for (int i=0;i<AlreadyMonitorVarVec.size();i++)
    {
        if (AlreadyMonitorVarVec[i] == targetInst)
        {
            errs()<<*targetInst<<" 已经插桩过了\n";
            return;
        }
    }

    IRBuilder<> IRbuilder(locateInst->getParent());
    IRbuilder.SetInsertPoint(locateInst->getNextNode());

    FunctionCallee functionCallee = NULL;

    Type *VoidTy = Type::getVoidTy(M->getContext());
    Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    Type *Int32Ty = Type::getInt32Ty(M->getContext());
    Type *Int64Ty = Type::getInt64Ty(M->getContext());
    Type *Int8Ty = Type::getInt8Ty(M->getContext());

    //状态机变量
    if (sceneType == StateMachineScene)
    {
        //确定使用哪个函数
        if (stateType == Var_Type_Int8  && targetInst->getType()->isIntegerTy(8))
        {
            functionCallee = M->getOrInsertFunction(StringRef(statemachine_int_8_funcname),FunctionType::get(VoidTy,{Int8Ty,Int32Ty},false));
        }if (stateType == Var_Type_Int8  && targetInst->getType()->isIntegerTy(64))
        {
            functionCallee = M->getOrInsertFunction(StringRef(statemachine_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
        }else if (stateType == Var_Type_Int32  && targetInst->getType()->isIntegerTy(32))
        {
            functionCallee = M->getOrInsertFunction(StringRef(statemachine_int_32_funcname),FunctionType::get(VoidTy,{Int32Ty,Int32Ty},false));
        }else if (stateType == Var_Type_Int64  && targetInst->getType()->isIntegerTy(64))
        {
            functionCallee = M->getOrInsertFunction(StringRef(statemachine_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
        }else if (stateType == Var_Type_Char)
        {
            functionCallee = M->getOrInsertFunction(StringRef(statemachine_char_funcname),FunctionType::get(VoidTy,{Int8PtrTy,Int32Ty},false));
        }


        Instruction *target = NULL;
        if (isa<Instruction>(targetInst)){
            target = cast<Instruction>(targetInst);
        }else{
            target = cast<Instruction>(locateInst);
        }
        
        if (target->getParent()->getName().contains_lower(StringRef("lor.lhs.false")) || target->getParent()->getName().contains_lower(StringRef("land.lhs.")) || target->getParent()->getName().contains_lower(StringRef("cond.")))
        {
            IRBuilder<> builder(target->getParent());
            builder.SetInsertPoint(target->getNextNode());

            CallInst *callInst = builder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            #ifdef DEBUG
            errs()<<"[MealyVar]"<<*callInst<<"\n";
            #endif
        }else{
            
            CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            #ifdef DEBUG
            errs()<<"[MealyVar]"<<*callInst<<"\n";
            #endif
        }
        curStateMachineNum++;
        return;
    }

    //普通变量
    if (sceneType == MultiScene && stateType == State_Int && targetInst->getType()->isIntegerTy(8))
    {
        functionCallee = M->getOrInsertFunction(StringRef(multistate_int_8_funcname),FunctionType::get(VoidTy,{Int8Ty,Int32Ty},false));
        CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
        callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
        curSubStateNum++;
        errs()<<"[SubVar_Int(8)]"<<*callInst<<"\n";
        return;
    }else if (sceneType == MultiScene && stateType == State_Int && targetInst->getType()->isIntegerTy(32))
    {
        functionCallee = M->getOrInsertFunction(StringRef(multistate_int_32_funcname),FunctionType::get(VoidTy,{Int32Ty,Int32Ty},false));
        CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
        callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
        curSubStateNum++;
        #ifdef DEBUG
        errs()<<"SubVar_Int(32)]"<<*callInst<<"\n";
        #endif
        return;
    }else if (sceneType == MultiScene && stateType == State_Int && targetInst->getType()->isIntegerTy(64))
    {
        functionCallee = M->getOrInsertFunction(StringRef(multistate_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
        CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
        callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
        
        curSubStateNum++;
        errs()<<"[SubVar_Int(64)]"<<*callInst<<"\n";
        return;
    }else if (sceneType == MultiScene && stateType == State_Char)
    {
        functionCallee = M->getOrInsertFunction(StringRef(multistate_char_funcname),FunctionType::get(VoidTy,{Int8PtrTy,Int32Ty},false));   
        CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
        callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));

        curSubStateNum++;
        errs()<<"[SubVar_Char]"<<*callInst<<"\n";
        return;
    }else if (sceneType == SingleScene && stateType == State_Int && targetInst->getType()->isIntegerTy(8))
    {
        Instruction *target = cast<Instruction>(targetInst);

        if (target->getParent()->getName().contains_lower(StringRef("lor.lhs.false")) || target->getParent()->getName().contains_lower(StringRef("land.lhs.")) || target->getParent()->getName().contains_lower(StringRef("cond.")))
        {   
            IRBuilder<> builder(target->getParent());
            builder.SetInsertPoint(target->getNextNode());
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_8_funcname),FunctionType::get(VoidTy,{Int8Ty,Int32Ty},false));

            CallInst *callInst = builder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            curSubStateNum++;
            errs()<<"[SubVar_Int(8)]"<<*callInst<<"\n";
        }else{
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_8_funcname),FunctionType::get(VoidTy,{Int8Ty,Int32Ty},false));
            CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            
            curSubStateNum++;
            errs()<<"[SubVar_Int(8)]"<<*callInst<<"\n";
        }
        return;
    }else if (sceneType == SingleScene && stateType == State_Int && targetInst->getType()->isIntegerTy(32))
    {
        Instruction *target = cast<Instruction>(targetInst);

        if (target->getParent()->getName().contains_lower(StringRef("lor.lhs.false")) || target->getParent()->getName().contains_lower(StringRef("land.lhs.")) || target->getParent()->getName().contains_lower(StringRef("cond.")))
        {
            IRBuilder<> builder(target->getParent());
            builder.SetInsertPoint(target->getNextNode());
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_32_funcname),FunctionType::get(VoidTy,{Int32Ty,Int32Ty},false));

            CallInst *callInst = builder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            curSubStateNum++;
            #ifdef DEBUG
            errs()<<"[SubVar_Int(32)]"<<*callInst<<"\n";
            #endif
            
        }else{
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_32_funcname),FunctionType::get(VoidTy,{Int32Ty,Int32Ty},false));

            CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));

            IRBuilder<> builder(locateInst->getParent());
            builder.SetInsertPoint(locateInst->getNextNode());
            curSubStateNum++;
            #ifdef DEBUG
            errs()<<"[SubVar_Int(32)]"<<*callInst<<"\n";
            #endif
        }

        return;
    }else if (sceneType == SingleScene && stateType == State_Int && targetInst->getType()->isIntegerTy(64))
    {
        Instruction *target = cast<Instruction>(targetInst);
        if (target->getParent()->getName().contains_lower(StringRef("lor.lhs.false")) || target->getParent()->getName().contains_lower(StringRef("land.lhs.")) || target->getParent()->getName().contains_lower(StringRef("cond.")))
        {
            IRBuilder<> builder(target->getParent());
            builder.SetInsertPoint(target->getNextNode());
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
            
            CallInst *callInst = builder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            errs()<<"[SubVar_Int(64)]"<<*callInst<<"\n";
            curSubStateNum++;

        }else{
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
            CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));

            IRBuilder<> builder(locateInst->getParent());
            builder.SetInsertPoint(locateInst->getNextNode());
            curSubStateNum++;
            errs()<<"SubVar_Int(64)]"<<*callInst<<"\n";
        }

        // functionCallee = M->getOrInsertFunction(StringRef(singlestate_int_64_funcname),FunctionType::get(VoidTy,{Int64Ty,Int32Ty},false));
        // CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
        // callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
        
        // errs()<<"[SingleScene-State_Int-IntegerTy(64)]"<<*callInst<<"\n";
        return;
    }else if (sceneType == SingleScene && stateType == State_Char && checkIfI8Pointer(targetInst))
    {
        //fixme:当目标指令属于If(a||b)中的b时，如果将监控函数插入到true basicblock中，则会出现didn't domain all uses情况，因为如果a为true，则也可以进入true bb; land.lhs
        Instruction *target = cast<Instruction>(targetInst);
        
        #ifdef DEBUG
        errs()<<"[**] "<<*locateInst<<"\t"<<*locateInst->getNextNode()<<"\n";
        errs()<<"[targetInst] "<<*targetInst<<"\n";
        #endif

        if (target->getParent()->getName().contains_lower(StringRef("lor.lhs.")) || target->getParent()->getName().contains_lower(StringRef("land.lhs.")) || target->getParent()->getName().contains_lower(StringRef("cond.")))
        {
            IRBuilder<> builder(target->getParent());
            builder.SetInsertPoint(target->getNextNode());
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_char_funcname),FunctionType::get(VoidTy,{Int8PtrTy,Int32Ty},false));
            CallInst *callInst = builder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));
            errs()<<"[SubVar_Char XXX]"<<*callInst<<"\n";
            curSubStateNum++;
        }else{
            functionCallee = M->getOrInsertFunction(StringRef(singlestate_char_funcname),FunctionType::get(VoidTy,{Int8PtrTy,Int32Ty},false));

            CallInst *callInst = IRbuilder.CreateCall(functionCallee,funcArgs);
            callInst->setMetadata(M->getMDKindID("nosanitize"),MDNode::get(M->getContext(),None));

            errs()<<"[SubVar_Char]"<<*callInst<<"\n";
            IRBuilder<> builder(locateInst->getParent());
            builder.SetInsertPoint(locateInst->getNextNode());
            curSubStateNum++;
        }
        
        return;

    }
    error = 1;
}


int identifyStateType(Instruction *instruction){
    //根据load类型调用不同的方法
    if (checkIfI8Pointer(instruction))
    {
        //i8*
        return State_Char;
    }else if (instruction->getType()->isIntegerTy())
    {
        //int
        return State_Int;
    }
    // errs()<<"instruction: "<<*instruction<<"\t"<<*instruction->getType()<<"\n";
    // assert(1==2);
    return -1;
}
// 比上面更详细，提供给状态机变量
int identifyStateVarType(Instruction *instruction){
    if (checkIfI8Pointer(instruction))
    {
        return Var_Type_Char;
    }else if (instruction->getType()->isIntegerTy(8))
    {
        return Var_Type_Int8;
    }else if (instruction->getType()->isIntegerTy(16))
    {
        return Var_Type_Int16;
    }else if (instruction->getType()->isIntegerTy(32))
    {
        return Var_Type_Int32;
    }else if (instruction->getType()->isIntegerTy(64))
    {
        return Var_Type_Int64;
    }
    return -1;
}

void addInstrumentInfoToVector(string filename,int codeline,string funcname,string semantics,int IsMealyVar){
    InstrumentInfo *one = new InstrumentInfo;
    one->filename = filename;
    one->codeline = codeline;
    one->funcname = funcname;
    one->semantics = semantics;
    one->IsMealyVar = IsMealyVar;
    AllInstrumentInfoVec.push_back(one);
}

void showAllInstrumentLog(){
    ofstream outfile;
    outfile.open("/tmp/results.txt",ios::out);
    // count the number of mealyvar and subvar, and dump the related information
    int MealyVarNum = 0, SubVarNum = 0;
    for (auto one : AllInstrumentInfoVec)
    {
        string location = one->filename + ":" + to_string(one->codeline);
        string funcname = one->funcname;
        string varname = one->semantics;

        if (one->IsMealyVar == StateMachineScene)
        {
            MealyVarNum++;
            outfile << "[MealyVar] varname: " << varname << " funcname: " << funcname << " location: " << location << "\n";
        }else if (one->IsMealyVar == SubStateScene)
        {
            SubVarNum++;
            outfile << "[SubVar] varname: " << varname << " funcname: " << funcname << " location: " << location << "\n";
        }
        
    }
    outfile<<"========================\n[Summary] MealyVarNum: "<<MealyVarNum<<"\tSubVarNum: "<<SubVarNum<<"\n";
    
    for (size_t i = 0; i < AllInstrumentInfoVec.size(); i++)
    {
        delete AllInstrumentInfoVec[i];
    }
}
// instead of checkArgsMeaningIfOK function
bool checkVarMeaning(vector<string> StateDict,string args){
    transform(args.begin(),args.end(),args.begin(),::tolower);

    // itearate all key words in StateDict to check if args is related protocol property
    for (string &meanval : StateDict)
    {
        // substring compare 
        if (int n = args.find(meanval) != string::npos)
        {
            // errs()<<"[checkVarMeaning] "<<args<<" vs "<<meanval<<"\n";
            return true;
        }
        // word2vec compare
        if (IfEnableWord2vec){
            float first[2000];
            if (extractStrVector(args.c_str(),word2vecdata.word,word2vecdata.vocab,word2vecdata.size,word2vecdata.M,first)){
                for (auto one : stateDictMap)
                {
                    float *second = one.second;
                    if (checkIfSimilarity(first,second,word2vecdata.size)){
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool checkFileIfInVector(vector<string> filevec,string filename){
    for (string &one : filevec)
    {
        if (filename.find(one) != string::npos)
        {
            return true;
        }
    }
    return false;
}



bool checkArgsMeaningIfOK(vector<vector<string>> MeanDict,string args,int switchFlag,int strictMode){
    // 检测是否是状态机变量的语义（单独处理）
    transform(args.begin(),args.end(),args.begin(),::tolower);
    if (switchFlag == StateMachine_Args_Pos || switchFlag == New_StateMachine_Args_Pos)
    {
        // errs()<<"字典长度： "<<MeanDict[StateMachine_Args_Pos].size()<<"\n";
        for (string &meanval : MeanDict[switchFlag])
        {
            //严格模式:目标字符串和字典必须完全一致，逐一比较
            if (strictMode != 0){
                if (meanval == args){
                    // errs()<<"dict content: "<<meanval<<"\t"<<args<<"\n";
                    return true;
                }
            }else{
                //非严格模式
                if (int n = args.find(meanval) != string::npos)
                {
                    // 反向过滤，黑名单TODO:完善黑名单
                    vector<string> blackList = {"len","size","count","time","point"};
                    for (string blackword : blackList)
                    {
                        if (int n = args.find(blackword) != string::npos)
                        {
                            return false;
                        }
                    }
                    if (IfEnableWord2vec){
                        errs()<<"word2vec校验\n";
                        float first[2000];
                        if (extractStrVector(args.c_str(),word2vecdata.word,word2vecdata.vocab,word2vecdata.size,word2vecdata.M,first)){
                            for (auto one :stateDictMap)
                            {
                                float *second = one.second;
                                if (checkIfSimilarity(first,second,word2vecdata.size)){
                                    return true;
                                }
                            }
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }
    //普通状态类型的检测
    for (string &meanval : MeanDict[switchFlag])
    {
        if (int n = args.find(meanval) != string::npos)
        {
            return true;
        }
    }
    #ifdef DEBUG
    errs()<<"参数："<<args<<"\n";
    #endif
    if (switchFlag==Protocol_Args_Pos && IfEnableWord2vec){
        // word2vec校验
        errs()<<"word2vec校验\n";
        float first[2000];
        if (extractStrVector(args.c_str(),word2vecdata.word,word2vecdata.vocab,word2vecdata.size,word2vecdata.M,first)){
            for (auto one :stateDictMap)
            {
                float *second = one.second;
                if (checkIfSimilarity(first,second,word2vecdata.size)){
                    return true;
                }
            }
        }
    }
    return false;
}


int CountLines(StringRef filename)
{
    ifstream ReadFile;
    int n=0;
    string tmp;
    ReadFile.open(filename.str());//ios::in 表示以只读的方式读取文件
    if(ReadFile.fail())//文件打开失败:返回0
    {
        return 0;
    }
    else//文件存在
    {
        while(getline(ReadFile,tmp,'\n'))
        {
            n++;
        }
        ReadFile.close();
        return n;
    }
}
 
string readContentFromTheFile(StringRef filename,int line)
{
    int lines,i=0;
    string temp;
    fstream file;
    file.open(filename.str());
    lines=CountLines(filename);
 
    if(line <= 0)
    {
        return "Error 1";    // 行数错误，不能为0或负数
    }
    if(file.fail())
    {
        return "Error 2"; // 文件不存在
    }
    if(line>lines)
    {
        return "Error 3"; // 行数超出文件长度
    }
    while(getline(file,temp) && i < line-1)
    {
        i++;
    }
    if (i<=3)
    {
        return "Error 4";   // 内容异常
    }
    
    file.close();
    return temp;
}

bool StartWith(string s1,string s2){
    return s1.compare(0, s2.size(), s2) == 0;
}

bool isFileExists_access(string& name){
    return (access(name.c_str(), F_OK) != -1);
}

void loadStateMachineCallGraph(){
    char *filepath = getenv("STATEMACHINE_FILE");
    if (filepath==NULL){
        return;
    }
    string filepahtstring = filepath;
    if (isFileExists_access(filepahtstring)==false){
        return;
    }
    ifstream file;
    file.open(filepahtstring,ios_base::in);
    if (!file.is_open())
    {
        return;
    }
    
    string s;
    while (getline(file,s))
    {
        stateMachineCallGraph.push_back(s);
    }
    file.close();
    errs()<<"stateMachineCallGraph size: "<<stateMachineCallGraph.size()<<"\n";
}

bool checkFuncInStateMachineCallGraph(StringRef filename){
    if (stateMachineCallGraph.size()==0)    //即未导入信息，默认为true
    {
        return true;
    }
    
    for (auto name :stateMachineCallGraph )
    {
        if (filename.contains(name))
        {
            return true;
        }
    }
    return false;
}

// 读取与初始化word2vector Model
void initialWord2VecModel(FILE *f,long long &words,long long &size,char *&vocab,float *&M,map<string,float*> &stateDictMap,vector<string> stateDicVec){
    const long long max_size = 2000;         // max length of strings
    // const long long N = 10;                  // number of closest words that will be shown
    const long long max_w = 50;              // max length of vocabulary entries

    char *file_name = NULL;
    file_name = getenv("MODEL_PATH");
    
    if (file_name==NULL || access(file_name,0) != F_OK){
        errs()<<"No word2vec model\n";
        return;
    }

    f = fopen(file_name, "rb");
    if (f == NULL) {
        errs()<<"Input file not found\n";
        return;
    }
    fscanf(f, "%lld", &words);
    fscanf(f, "%lld", &size);
    vocab = (char *)malloc((long long)words * max_w * sizeof(char));
    
    // char *bestw[N];
    int a,b;
    float len;
    // for (a = 0; a < N; a++) bestw[a] = (char *)malloc(max_size * sizeof(char));
    M = (float *)malloc((long long)words * (long long)size * sizeof(float));
    if (M == NULL) {
        // errs()<<"Cannot allocate memory: %lld MB    %lld  %lld\n", (long long)words * size * sizeof(float) / 1048576, words, size);
        return;
    }
    for (b = 0; b < words; b++) {
        a = 0;
        while (1) {
            vocab[b * max_w + a] = fgetc(f);
            if (feof(f) || (vocab[b * max_w + a] == ' ')) break;
            if ((a < max_w) && (vocab[b * max_w + a] != '\n')) a++;
        }
        vocab[b * max_w + a] = 0;
        for (a = 0; a < size; a++) fread(&M[a + b * size], sizeof(float), 1, f);
        len = 0;
        for (a = 0; a < size; a++) len += M[a + b * size] * M[a + b * size];
        len = sqrt(len);
        for (a = 0; a < size; a++) M[a + b * size] /= len;
    }
    fclose(f);
    //初始化state字典中的词汇向量
    for (string word : stateDicVec){
        float *first = (float*)malloc(sizeof(float) * max_size);
        if (extractStrVector(word.c_str(),words,vocab,size,M,first)){
            errs()<<"first addr "<<first<<"\n";
            stateDictMap.insert(pair<string,float*>(word,first));
        }
    }
    IfEnableWord2vec = true;
}
//从模型中提取目标字符串的向量,返回给参数first[]
bool extractStrVector(const char *str,long long words,char *vocab,long long size,float *M,float first[]){
    const long long max_size = 2000;         // max length of strings
    const long long max_w = 50;              // max length of vocabulary entries

    char st1[max_size];
    strcpy(st1,str);
    long long b;
    for (b = 0; b < words; b++) if (!strcmp(&vocab[b * max_w], st1)) break;
    errs()<<"Word: "<<st1<<"  Position in vocabulary: "<<b<<"\n";
    if (b == -1)
    {
        printf("Out of dictionary word!\n");
        return false;
    }
    // float first[max_size];
    int a;
    for (a = 0; a < size; ++a) {
        first[a] = M[a + b * size];
    }
    return true;
}
//检查两个字符串对应向量first和second之间的余弦相似度，
bool checkIfSimilarity(float* first,float* second,long long size){
    float product1 = 0.0, product2 = 0.0, sum12 = 0.0;
    int a;
    for (a = 0; a < size; ++a) {
        product1 = product1 + first[a] * first[a];
        product2 = product2 + second[a] * second[a];
        sum12 = sum12 + first[a] * second[a];
    }
    float len1 = sqrt(product1);
    float len2 = sqrt(product2);
    float similarity = sum12/(len1 * len2);
    errs()<<"the similarity between these two words is "<<similarity<<"\n";
    if (similarity >= 0.5){
        return true;
    }
    return false;
}
//关闭word2vec模型
void closeWord2VecModel(Word2VecData *word2vecdata,map<string,float*> &stateDictMap){
    free(word2vecdata->vocab);
    free(word2vecdata->M);
    for (auto &one : stateDictMap)
    {
        errs()<<"one: "<<one.first<<"\t"<<one.second<<"\n";
        free(one.second);
    }
}
// 提取Switch case value值
void extractSwCaseValue(SwitchInst *swInst,vector<int> &SwCaseValueVec){
    for (auto &swCase : swInst->cases())
    {
        int caseVal = swCase.getCaseValue()->getZExtValue();
        #ifdef DEBUG
        errs()<<"extract case value "<<caseVal<<"\n";
        #endif
        SwCaseValueVec.push_back(caseVal);
    }
}

StringRef delLLVMIRInfoForVar(StringRef varName){
    // errs()<<"function delLLVMIRInfoForVar: "<<varName<<"\n";
    if (varName.empty()){
        return "Null";
    }
    // 以.addr结尾,则直接删除
    if (varName.endswith(StringRef(".addr")))
    {
        return varName.substr(0,varName.size()-5);  
    }
    int i;
    for (i = varName.size()-1; i >= 0; i--)
    {
        if (!(varName[i]>='0'&&varName[i]<='9'))
        {
            break;
        }
    }
    return varName.substr(0,i+1);
}

string getVariableNameWithParent(Value *variable){
    string varName = delLLVMIRInfoForVar(variable->getName()).str();

    if (GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(variable))
    {
        
        Instruction *gepOpInst = dyn_cast<Instruction>(gepinst->getOperand(0));
        // errs()<<"variable: "<<*variable<<"\tgetpOpInst: "<<*gepOpInst<<"\n";
        if (gepOpInst && isa<LoadInst>(gepOpInst)){
            LoadInst *prev_load = cast<LoadInst>(gepOpInst);
            Value *parent_var = prev_load->getOperand(0);
            Instruction *parent_var_inst = dyn_cast<Instruction>(parent_var);
            if (parent_var_inst){
                string parent_str = delLLVMIRInfoForVar(parent_var->getName());
                string results = parent_str + "." + varName;
                // errs()<<"[getVariableNameWithParent] "<<results<<"\n";
                return results;
            }
        }
    }
    // errs()<<"[getVariableNameWithParent] "<<varName<<"\n";
    return delLLVMIRInfoForVar(varName).str();
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

bool checkIsIFCondition(BasicBlock *basicblock){
// if (basicblock.getName().startswith("if.end") && succ_size(&basicblock) == 2 && checkSuccStartWithIfNumber(&basicblock) == 2)
// {
//     continue;
// }
    if (succ_size(basicblock)==2)
    {
        //均不以if开头
        if (checkSuccStartWithIfNumber(basicblock)==0)
        {
            return false;
        }
        if (checkSuccStartWithIfNumber(basicblock)==2)
        {
            return true;
        }
        BasicBlock *Succ1 = basicblock->getTerminator()->getSuccessor(0);
        BasicBlock *Succ2 = basicblock->getTerminator()->getSuccessor(1);
        if (Succ1->getName().startswith(StringRef("if.then")) && (Succ2->getName().startswith(StringRef("lor"))))
        {
            return true;
        }
        
        // if (basicblock->getName().startswith("if.end") && checkSuccStartWithIfNumber(basicblock) == 2)
        // {
        //     return true;
        // }
    }
    return false;
}

// load the info of mealy machine variables that are identified from files
void loadMealyMachineStateVarInfo(vector<StateVar> &MealyStateVarVec,vector<StateVar> &SubStateVarVec){
    string inputFilename = "./StateVarInfo.txt";
    ifstream infile(inputFilename);
    if (!infile.is_open())
    {
        errs()<<"[loadMealyMachineStateVarInfo] open file StateVarInfo failed!\n";
        return;
    }
    string line;
    regex re(R"(SceneType: (\S+)\s+VarName: (\S+)\s+Location: (\S+)\s+Funcname: (\S+)\s+\[b (\S+)\])");
    smatch match;

    while (getline(infile, line))
    {
        errs()<<"line: "<<line<<"\n";
        if (line.find("==========") != string::npos){
            break;
        }

        if (line.find("[MealyVar]") != string::npos) {
            regex re2(R"(\[MealyVar\]\s+(.+))");
            smatch match2;
            if (regex_search(line, match2, re2))
            {
                string content = match2[1].str();
                if (regex_search(content, match, re))
                {
                    StateVar stateVar;
                    stateVar.FuncName = match[4].str();
                    stateVar.Location = match[3].str();
                    stateVar.VarName = match[2].str();
                    stateVar.SceneType = match[1].str();
                    MealyStateVarVec.push_back(stateVar);
                    // errs() << "SceneType: " << match[1].str() << "\n";
                    // errs() << "VarName: " << match[2].str() << "\n";
                    // errs() << "Location: " << match[3].str() << "\n";
                    // errs() << "Funcname: " << match[4].str() << "\n";
                }
            }
        } else if (regex_search(line, match,re))
        {
            StateVar stateVar;
            stateVar.FuncName = match[4].str();
            stateVar.Location = match[3].str();
            stateVar.VarName = match[2].str();
            stateVar.SceneType = match[1].str();
            SubStateVarVec.push_back(stateVar);
            // errs() << "SceneType: " << match[1].str() << "\n";
            // errs() << "VarName: " << match[2].str() << "\n";
            // errs() << "Location: " << match[3].str() << "\n";
            // errs() << "Funcname: " << match[4].str() << "\n";
            // errs() << "b: " << match[5].str() << "\n";
        }
    }
    errs() << "loadMealyMachineStateVarInfo done! MealyStateVarVec.size() = " << MealyStateVarVec.size() << "\t "
           << "SubStateVarVec.size() = " << SubStateVarVec.size() << "\n";
}

int checkInstructIfStateVar(Instruction *instruction,vector<StateVar> StateVarVec, int type) {
    string sceneType;
    switch (type)
    {
        case 0:
            sceneType = "switch";
            break;
        case 1:
            sceneType = "if";
            break;
        case 2:
            sceneType = "indirect";
            break;
    }
    string funcname = instruction->getFunction()->getSubprogram()->getName();
    string filename = instruction->getDebugLoc()->getDirectory().str() + "/" + instruction->getDebugLoc()->getFilename().str();
    int line = instruction->getDebugLoc()->getLine();
    string location = filename + ":" + to_string(line);
    errs()<<"StateVarVec.size: "<<StateVarVec.size()<<"\n";
    errs()<<"location: "<<location<<"\n";
    for (auto stateVar : StateVarVec)
    {
        #ifdef DEBUG
        errs()<<"[checkInstructIfStateVar]"<<location<<" vs "<<stateVar.Location<<"\t"
              << funcname<<" vs "<<stateVar.FuncName<<"\t"
              << sceneType<<" vs "<<stateVar.SceneType<<"\n";
        #endif

        if (stateVar.SceneType == sceneType && stateVar.FuncName == funcname && stateVar.Location == location)
        {
            errs()<<"[checkInstructIfStateVar] find MealyStateVar: "<<stateVar.VarName<<"!\n";
            return 1;
        }   
    }
    return 0;
}