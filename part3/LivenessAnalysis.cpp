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


class LivenessInfo : public Info {


public:
   vector<unsigned> v_info;

   void print() {
      for(unsigned v : v_info) {
         errs() << v << "|";
      }
      errs() << "\n";
   }

   void removeInfo(unsigned i) {
      v_info.erase(std::remove(v_info.begin(), v_info.end(), i), v_info.end());
   }

   void addInfo(unsigned i) {
      v_info.push_back(i);
      sort(v_info.begin(), v_info.end());
      v_info.erase(std::unique(v_info.begin(), v_info.end()), v_info.end());
   }

   static bool equals(LivenessInfo *info1, LivenessInfo *info2) {
      return info1->v_info == info2->v_info;
   }


   static LivenessInfo* join(LivenessInfo *info1, LivenessInfo *info2, LivenessInfo *result) {
    
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
class LivenessAnalysis : public DataFlowAnalysis<Info, Direction> {

   private:

      void initializeForwardMap(Function * func) {
         DataFlowAnalysis<Info, Direction>::initializeForwardMap(func);
      }

      void initializeBackwardMap(Function * func) {
      }

      unsigned getInstrType(Instruction *I) {

         if(strcmp(I->getOpcodeName(), "phi") == 0)
            return 3;

         if(I->isBinaryOp()                                  || 
            I->isShift()                                     ||
            strcmp(I->getOpcodeName(), "alloca")        == 0 ||
            strcmp(I->getOpcodeName(), "load")          == 0 ||
            strcmp(I->getOpcodeName(), "getelementptr") == 0 ||
            strcmp(I->getOpcodeName(), "icmp")          == 0 ||
            strcmp(I->getOpcodeName(), "fcmp")          == 0 ||
            strcmp(I->getOpcodeName(), "select")        == 0)
            return 1;

         return 2;
      }

      void flowfunction(Instruction * I,
                        std::vector<unsigned> & IncomingEdges,
                        std::vector<unsigned> & OutgoingEdges,
                        std::vector<Info *> & Infos) {

         if(I == NULL) return;

         Info *newInfo = new Info();

         unsigned index = this->getInstrToIndex(I);
         for(auto i : IncomingEdges) {
            LivenessInfo * oldInfo = this->getEdgeToInfo(make_pair(i, index));
            LivenessInfo::join(newInfo, oldInfo, newInfo);
         }

         unsigned instrType = getInstrType(I);
         int to = I->getNumOperands();
         switch(instrType) {
            case 1:  
               for(int from = 0; from < to; from++) {
                  if(llvm::dyn_cast<Instruction>(I->getOperand(from))) {
                     Instruction *instr = llvm::dyn_cast<Instruction>(I->getOperand(from));
                     if(instr != NULL) {
                        newInfo->addInfo(this->getInstrToIndex(instr));
                     }
                  }   
               }
               newInfo->removeInfo(index);
               break;

            case 2:
               for(int from = 0; from < to; from++) {
                  if(llvm::dyn_cast<Instruction>(I->getOperand(from))) {
                     Instruction *instr = llvm::dyn_cast<Instruction>(I->getOperand(from));
                     if(instr != NULL) {
                        newInfo->addInfo(this->getInstrToIndex(instr));
                     }
                  }   
               }
               break;

            case 3:  
               newInfo->removeInfo(this->getInstrToIndex(I));
               for(auto ib = I->getParent()->begin(), ie = I->getParent()->end(); ib != ie; ib++) {
                  Instruction *instr = &*ib;
                  if(isa<PHINode>(instr)) {
                     newInfo->removeInfo(this->getInstrToIndex(instr));
                  }
               }
               break;
         }


         for(unsigned i = 0; i < OutgoingEdges.size(); i++) {

            if(instrType == 3) {

               Info *tempInfo = new Info();
               *tempInfo = *newInfo;
               
               for(auto ib = I->getParent()->begin(), ie = I->getParent()->end(); ib != ie; ib++) {
                  if(isa<PHINode>(&*ib)) {
                     PHINode *pn = llvm::dyn_cast<PHINode>(&*ib);   
                     BasicBlock *label = (this->getIndexToInstr(OutgoingEdges[i]))->getParent();
                     
                     for(unsigned index = 0; index < pn->getNumIncomingValues(); index++) {
                        if(label == pn->getIncomingBlock(index)) {
                           Instruction *instr = dyn_cast<Instruction>(pn->getIncomingValue(index));
                           if(instr != NULL) {
                              tempInfo->addInfo(this->getInstrToIndex(instr));
                           }
                        }
                     }
                  }
               }
               Infos.push_back(tempInfo);
            }
            else {
               Infos.push_back(newInfo);
            }
         
         }

      } // end flowfunction

   public:
      LivenessAnalysis(Info &bottom, Info &initState) : DataFlowAnalysis<Info, Direction>(bottom, initState) {}

};


namespace {
   struct LivenessAnalysisPass : public FunctionPass {
      public:
         static char ID;
         LivenessAnalysisPass() : FunctionPass(ID) {}
         bool runOnFunction(Function &F) override { 
            LivenessInfo *bott = new LivenessInfo();
            LivenessInfo *init = new LivenessInfo();

            LivenessAnalysis<LivenessInfo, false> analysis(*bott, *init);

            analysis.runWorklistAlgorithm(&F);
            analysis.print();
            return false;
         }
   }; 
}

char LivenessAnalysisPass::ID = 0;
static RegisterPass<LivenessAnalysisPass> X("cse231-liveness",
                                            "Developed to test LLVM and docker",
                                            false /* Only looks at CFG */,
                                            false /* Analysis Pass */);