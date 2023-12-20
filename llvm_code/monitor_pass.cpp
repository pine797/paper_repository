#include "./include/llvmheader.h"
#include "./include/cppheader.h"
#include "./include/funcpointer.h"
#include "./include/callgraph.h"
#include "./include/commonvar.h"
#include "./include/globalvar.h"
#include "./include/switchmodule.h"

using namespace llvm;
using namespace std;


cl::opt<int> LLVM_OPTION_STEP("step", cl::Required,cl::desc("1: extract candidate mealy variable,  2: identify and trace all state variable"), cl::value_desc("int"));

#define FIND_CANDIDATE_MEALY_VAR 1
#define INSTRUMENT_STATE_VAR 2

extern vector<Value*> AlreadyMonitorVarVec;


extern Word2VecData word2vecdata;
map<string,float*> stateDictMap;
extern bool IfEnableWord2vec;


extern vector<string> stateMachineCallGraph;

class AutoMonitorModulePass : public ModulePass
{
public:
    static char ID;

    vector<string> StateVariableDictionary; 
    map<Function*,int> CallGraphMap; 

    
    vector<StateVar> MealyStateVarVec;
    vector<StateVar> SubStateVarVec; 

    explicit AutoMonitorModulePass() : ModulePass(ID) {
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
    int OptStep = LLVM_OPTION_STEP;
    
    loadStateMachineCallGraph();
    
    loadStateVariableDictFile(StateVariableDictionary);
    
    initialWord2VecModel(word2vecdata.f,word2vecdata.word,word2vecdata.size,word2vecdata.vocab,word2vecdata.M,stateDictMap,StateVariableDictionary);
    
    extractCallGraph(M,CallGraphMap);

    
    vector<StringRef> FuncPointerVec;   
    vector<StringRef> FuncPointerUsedVec;   
    vector<StringRef> FuncUsedTargetStructVec; 
    vector<string> FunPointerArrayElementTypeStringVec; 
    vector<Type*> FunPointerArrayElementTypeValueVec;   

        
    if (OptStep == FIND_CANDIDATE_MEALY_VAR) {      
        

        extractGlobalVar(M, FuncPointerVec,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
        if (FuncPointerVec.size() > 0)
        {
            extractGlobalStructContainsTargetType(M,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
            
            extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(M,FuncPointerUsedVec,FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec,CallGraphMap);

            checkAllCallSite_Candidate_Mealy(M,FuncPointerVec,FuncPointerUsedVec,FuncUsedTargetStructVec,CallGraphMap,stateMachineCallGraph);
        }
        
        handleBranch_Candidate_Mealy(M,CallGraphMap);
    
    }else if(OptStep == INSTRUMENT_STATE_VAR){  
        
        loadMealyMachineStateVarInfo(MealyStateVarVec,SubStateVarVec);
        
        #ifdef DEBUG
        errs()<<"MealyStateVarVec.size():"<<MealyStateVarVec.size()<<"\t"<<"SubStateVarVec.size():"<<SubStateVarVec.size()<<"\n";
        #endif
        
        extractGlobalVar(M, FuncPointerVec,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
        if (FuncPointerVec.size() > 0)
        {
            extractGlobalStructContainsTargetType(M,FunPointerArrayElementTypeStringVec,FunPointerArrayElementTypeValueVec);
            extractFuncPointerUsedFuncnameAndFunctionContainTargetStructType(M,FuncPointerUsedVec,FunPointerArrayElementTypeValueVec,FuncUsedTargetStructVec,CallGraphMap);
            saveStateVarInDirectScene(M,FuncPointerVec,FuncPointerUsedVec,FuncUsedTargetStructVec,MealyStateVarVec,SubStateVarVec,CallGraphMap,stateMachineCallGraph);
            handleCandidateObjForFuncPointer(M,StateVariableDictionary,FuncPointerVec);
        }
    
        
        handleStateVarInSwitchScene(M,CallGraphMap,StateVariableDictionary,MealyStateVarVec);
    
        handleStateVarInIfScene(M,CallGraphMap,StateVariableDictionary,MealyStateVarVec);

        
        iterateAllSwAndIfCondition(M,CallGraphMap,StateVariableDictionary);

    }

    
    if (IfEnableWord2vec){
        closeWord2VecModel(&word2vecdata,stateDictMap);
    }
    
    showAllInstrumentLog();
    
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
