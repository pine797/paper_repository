#include "./include/llvmheader.h"
#include "./include/cppheader.h"
#include "./include/funcpointer.h"
#include "./include/callgraph.h"
#include "./include/commonvar.h"
#include "./include/globalvar.h"
#include "./include/switchmodule.h"

using namespace llvm;
using namespace std;

extern vector<Value*> AlreadyMonitorVarVec;
extern int PROGRAM_LANG_FLAGS;

//word2vec所需要的变量
extern Word2VecData word2vecdata;
map<string,float*> stateDictMap;
extern bool IfEnableWord2vec;

extern vector<string> stateMachineCallGraph;

class AutoMonitorModulePass : public ModulePass
{
public:
    static char ID;

    vector<string> dispatchFuncDic;//模拟NLP获取的派遣函数的名称
    vector<string> FuncArgsVec;//模拟NLP获取的函数参数名称
    vector<string> stateDicVec; //普通的状态变量字典
    vector<string> stateMachineDicVec; //state machine状态变量字典
    vector<string> new_stateMachineDicVec;  //新发现的字典存在这里,用于泛化识别时使用

    explicit AutoMonitorModulePass() : ModulePass(ID) {
        dispatchFuncDic = {"handle","dispatch","process","command","cmd","message","packet","parse","msg","request","machine"}; //模拟NLP获取的派遣函数的关键字
        FuncArgsVec = {"param","cmd","command"}; //模拟NLP获取的函数参数名称
        // stateDicVec = {"qos","cmd","command","state","version","keepalive","type","code","flag","response","request","reply","protocol","category","mode","operation","data","msg","status","method"};
        stateDicVec = {"data","qos","state","status","msg","type","category","version","command","cmd","method","code","request","req","response","cmdname","operation","pdutype","result","attribute","flag","mode","attr"};//TODO:msg_type和msg.type区别非常小，建议引入字符串相似算法：编辑距离
        stateMachineDicVec = {"data","state","status","stream.state","category","command","cmd","method","code","request","response","responsecode","statuscode","operation","pdutype","response.state","assoc.type","association.type","request.type","msg.type","msg_type"};
        new_stateMachineDicVec = {};
    }

    StringRef getPassName() const override
    {
        return "AutoMonitorModulePass";
    }

    bool runOnModule(Module &F) override;
};

char AutoMonitorModulePass::ID = 0;

bool AutoMonitorModulePass::runOnModule(Module &M)
{
    // ================预处理阶段================
    // 检查环境变量，确定IR是C or CPP
    PROGRAM_LANG_FLAGS = 1; //默认为CPP模式
    
    // 初始化读取StateMachine的检测范围
    loadStateMachineCallGraph();

    // 初始化读取模型文件
    initialWord2VecModel(word2vecdata.f,word2vecdata.word,word2vecdata.size,word2vecdata.vocab,word2vecdata.M,stateDictMap,stateDicVec);

    // 0.模块所需变量
    map<Function*,int> CallGraphMap_State,CallGraphMap_All; //存储callgraph结果
    // 1.提取callgraph范围
    extractCallGraph(M,CallGraphMap_State,CallGraphMap_All);
    errs()<<"extractCallGraph size: "<<CallGraphMap_All.size()<<"\n";

    // ================变量检测阶段================
    // 0.模块所需变量
    
    // （函数指针模块）
    vector<vector<string>> MeaningDict = {dispatchFuncDic,FuncArgsVec,stateDicVec,stateMachineDicVec,new_stateMachineDicVec}; //存储dispatch函数、函数参数的语义字典
    vector<StringRef> FuncPointerVec;   //保存内部有函数指针的GlobalVariable
    vector<StringRef> FuncPointerUsedVec;   //保存使用了函数指针的Function数组
    vector<StringRef> FuncUsedTargetStructVec; //保存使用了目标结构体类型的Function数组
    vector<string> FunPointerArrayElementTypeStringVec; //union类型提取时候会使用到，与dwarf info结合起来，因为指令和debuginfo中的名称不是一一对应的
    vector<Type*> FunPointerArrayElementTypeValueVec;   //存储包含函数指针数组基本类型的结构体类型，globalvariable层面检测时会使用到

    // goto SwitchModule;
    
    // [1] iterate all globalvariabels to find the one that stored function pointer
    extractGlobalVar(M, FuncPointerVec,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);

    //1.函数指针检测模块：if Module hava a global array that stored function pointer , then start function pointer detection
    errs() << "指针数组长度是" << FuncPointerVec.size() << "\n";
    // TODO:openssl在这卡住
    if (FuncPointerVec.size() > 0)
    {
        extractGlobalStructContainsTargetType(M,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
        // store the name of function that has used function pointer in its method
        extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(M,FuncPointerUsedVec,FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec);   
        errs()<<"FuncPointerUsedVec size: "<<FuncPointerUsedVec.size()<<"\n"; 
        errs()<<"FunPointerArrayElementTypeStringVec size: "<<FunPointerArrayElementTypeStringVec.size()<<"\n";
        errs()<<"FunPointerArrayElementTypeValueVec size: "<<FunPointerArrayElementTypeValueVec.size()<<"\n";
        errs()<<"FuncUsedTargetStructVec size: "<<FuncUsedTargetStructVec.size()<<"\n";

        checkAllCallSite(M, FuncPointerVec,FuncPointerUsedVec,MeaningDict,FuncUsedTargetStructVec,CallGraphMap_All);

        handleCandidateObjForFuncPointer(M,MeaningDict,FuncPointerVec);
    }
    // return false;
    //2.Switch/IF检测模块：if not , then only start switch or if else detection
    // vector<Instruction*> NormalVariableVec;

    handleSwitch(M,CallGraphMap_All,MeaningDict);
    goto End;
// SwitchModule:
    handleIfElse(M,CallGraphMap_All,MeaningDict);
    //3.Other变量检测模块
    iterateAllSwAndIfCondition(M,CallGraphMap_All,MeaningDict);

    handleNormalSwitch(M,CallGraphMap_All,MeaningDict);
End:
    // 关闭Word2vec模型
    if (IfEnableWord2vec){
        closeWord2VecModel(&word2vecdata,stateDictMap);
    }

    //导出日志信息
    showAllInstrumentLog();
    errs()<<"被监控变量的个数: "<<AlreadyMonitorVarVec.size()<<"\n";
    return true;
}

static void registerInvsCovPass(const PassManagerBuilder &,
                                legacy::PassManagerBase &PM)
{

    PM.add(new AutoMonitorModulePass());
}

static RegisterStandardPasses RegisterInvsCovPass(
    PassManagerBuilder::EP_ModuleOptimizerEarly, registerInvsCovPass);


static RegisterPass<AutoMonitorModulePass>
    X("autoMonitor-instrument", "AutoMonitorModulePass",
      false,
      false);
