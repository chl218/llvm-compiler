#include "231DFA.h"

#include "llvm/Pass.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>

using namespace llvm;
using namespace std;


class ReachingInfo : public Info {


public:
   vector<unsigned> v_info;

   void print() {
      for(unsigned v : v_info) {
         errs() << v << "|";
      }
      errs() << "\n";
   }

   void addInfo(unsigned i) {
      v_info.push_back(i);
      sort(v_info.begin(), v_info.end());
      v_info.erase(std::unique(v_info.begin(), v_info.end()), v_info.end());
   }

   static bool equals(ReachingInfo *info1, ReachingInfo *info2) {
      //errs() << info1->v_info.size() << " " << info2->v_info.size() << "\n";
      return info1->v_info == info2->v_info;
   }


   static ReachingInfo* join(ReachingInfo *info1, ReachingInfo *info2, ReachingInfo *result) {
    
      if(result == NULL || info1 == NULL || info2 == NULL) return NULL;

      result->v_info.reserve(info1->v_info.size() + info2->v_info.size());
      result->v_info.insert(result->v_info.end(), info1->v_info.begin(), info1->v_info.end());
      result->v_info.insert(result->v_info.end(), info2->v_info.begin(), info2->v_info.end());

      sort(result->v_info.begin(), result->v_info.end());
      result->v_info.erase(std::unique(result->v_info.begin(), result->v_info.end()), result->v_info.end());
      return result;
   }   

};


template <class Info, bool Direction>
class ReachingAnalysis : public DataFlowAnalysis<Info, Direction> {

   private:

      void initializeForwardMap(Function * func) {
         DataFlowAnalysis<Info, Direction>::initializeForwardMap(func);
      }

      void initializeBackwardMap(Function * func) {
      }

      unsigned getInstrType(Instruction *I) {

         if(I->isBinaryOp()                                  || 
            I->isShift()                                     ||
            strcmp(I->getOpcodeName(), "alloca")        == 0 ||
            strcmp(I->getOpcodeName(), "load")          == 0 ||
            strcmp(I->getOpcodeName(), "getelementptr") == 0 ||
            strcmp(I->getOpcodeName(), "icmp")          == 0 ||
            strcmp(I->getOpcodeName(), "fcmp")          == 0 ||
            strcmp(I->getOpcodeName(), "select")        == 0)
            return 1;


         if(strcmp(I->getOpcodeName(), "br")      == 0  || 
            strcmp(I->getOpcodeName(), "switch")  == 0  ||
            strcmp(I->getOpcodeName(), "store")   == 0)
            return 2;

         if(strcmp(I->getOpcodeName(), "phi") == 0)
            return 3;

         return 0;
      }

      void flowfunction(Instruction * I,
                        std::vector<unsigned> & IncomingEdges,
                        std::vector<unsigned> & OutgoingEdges,
                        std::vector<Info *> & Infos) {

         //errs() << "stuck in flowlimbo~~~~\n";
         if(I == NULL) return;

         Info *newInfo = new Info();

         unsigned index = this->getInstrToIndex(I);
         for(auto i : IncomingEdges) {
            ReachingInfo * oldInfo = this->getEdgeToInfo(make_pair(i, index));
            ReachingInfo::join(newInfo, oldInfo, newInfo);
         }

         unsigned instrType = getInstrType(I);
         switch(instrType) {
            case 1:  newInfo->addInfo(index); break;
            case 3:  
               newInfo->addInfo(index);
               unsigned nextIndex = index+1;
               while(1) {
                  if(getInstrType(this->getIndexToInstr(nextIndex)) != 3) break;
                  newInfo->addInfo(nextIndex);
                  nextIndex++;
               }
               break;
         }
         for(unsigned i = 0; i < OutgoingEdges.size(); i++) {
            Infos.push_back(newInfo);
         }

         //errs() << "yay... out of flowlimbo...\n";
      } // end flowfunction

   public:
      ReachingAnalysis(Info &bottom, Info &initState) : DataFlowAnalysis<Info, Direction>(bottom, initState) {}

};


namespace {
   struct ReachingDefinitionAnalysisPass : public FunctionPass {
      public:
         static char ID;
         ReachingDefinitionAnalysisPass() : FunctionPass(ID) {}
         bool runOnFunction(Function &F) override { 
            ReachingInfo *bott = new ReachingInfo();
            ReachingInfo *init = new ReachingInfo();
            ReachingAnalysis<ReachingInfo, true> analysis(*bott, *init);

            analysis.runWorklistAlgorithm(&F);
            analysis.print();
            return false;
         }
   }; 
}

char ReachingDefinitionAnalysisPass::ID = 0;
static RegisterPass<ReachingDefinitionAnalysisPass> X("cse231-reaching",
                                                      "Developed to test LLVM and docker",
                                                      false /* Only looks at CFG */,
                                                      false /* Analysis Pass */);