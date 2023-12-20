#include "./include/llvmheader.h"
#include "./include/cppheader.h"
#include "./include/callgraph.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace std;

// 函数栈日志文件路径
static cl::opt<string> FilePath("filename", cl::desc("global filepath for function stack content"),cl::value_desc("file name"));
// 向下遍历的层数
static cl::opt<int> CallGraphLevel("level", cl::desc("the level of callgraph from the entry function"),cl::init(3));


class AutoMonitorModulePass : public ModulePass
{
public:
    static char ID;

    vector<string> dispatchFuncDic;//模拟NLP获取的派遣函数的名称

    explicit AutoMonitorModulePass() : ModulePass(ID) {
        dispatchFuncDic = {"handle","dispatch","process","command","cmd","message","packet","parse","msg","request","machine"}; //模拟NLP获取的派遣函数的关键字
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
    // 0.模块所需变量
    map<Function*,int> CallGraphMap; //存储callgraph结果
    vector<FunctionStackInfo> FuncStackVec;
    int SkipFlags = 0;  //入口函数中是否跳过select之前的代码
    int CallGraphLevel = 3;
    errs()<<"CallGraphLevel "<<CallGraphLevel<<"\n";

    // 1. 读取函数栈内容
    string filename = FilePath.c_str();     //函数栈路径
    readRecvFuncStackInfo(filename,FuncStackVec);
    // return true;
    // // 2. 确定入口函数（函数栈内容 and select + 多进程）
    // for (auto &function : M.getFunctionList())
    // {   
    //     errs()<<function.getName()<<"\t"<<function.isDeclaration()<<"\n";
    // }
    // test(&M);
    Function *entryFunction = findStartFunction(&M,FuncStackVec,SkipFlags);
    errs()<<"entryFunction: "<<entryFunction->getName()<<"\n";
    // return true;
    // 3.提取callgraph范围
    extractCallGraph(M,CallGraphMap,FuncStackVec,entryFunction,CallGraphLevel,SkipFlags);
    // 4.补充检测（对于间接调用的，从范围之外的函数中再筛选一批函数作为callgraph范围，白名单筛选）
    extractOtherCallGraph(M,CallGraphMap);
    // return true;
    // 5.到处函数列表中的filename（暂时性的，为了快速复用现在的工具去验证效果区别是否大）
    map<string,int> filenameMap;
    vector<string> functionVec;
    for (auto iter : CallGraphMap){
        string funcname;
        string filename;
        if (iter.first->getSubprogram())
        {
            funcname = iter.first->getSubprogram()->getName();
            filename = iter.first->getSubprogram()->getFilename();
        }else{
            funcname = iter.first->getName();
        }
        filenameMap.insert(pair<string,int>(filename,0));
        functionVec.push_back(funcname);
    }
    ofstream outFile;
    outFile.open("filelist.txt");
    for (auto iter:filenameMap)
    {
        int pos = iter.first.rfind("/");
        if (pos!=string::npos)
        {
            int length = iter.first.length();
            string substr = iter.first.substr(pos+1,length-pos);
            errs()<<"pos: "<<pos<<"\tlength: "<<length<<"\t"<<substr<<"\n";
            outFile<<substr<<"\n";
            // errs()<<"filename: "<<iter.first<<"\t"<<substr<<"\n";
        }else{
            errs()<<"filename: "<<iter.first<<"\n";
            outFile<<iter.first<<"\n";
        }
    }
    outFile.close();
    // ofstd/libsrc/oflist.cc
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
