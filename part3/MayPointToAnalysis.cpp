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


#define DEBUG_INFO 0
#define DEBUG_FLOW 0

typedef pair<char, unsigned> pointerInfo_t;


class MayPointToInfo : public Info {


public:

   map<pointerInfo_t, vector<pointerInfo_t>> info;

   void print() {
      for(auto &pointer : info) {
         errs() << pointer.first.first << pointer.first.second << "->(";
         for(auto &pointee : pointer.second) {
            errs() << pointee.first << pointee.second << "/";
         }
         errs() << ")|";
      }
      errs() << "\n";
   }

   void addInfo(pointerInfo_t pointer, pointerInfo_t pointee) {
      auto iter = info.find(pointer);
      if(iter != info.end()) {
         iter->second.push_back(pointee);
         sort(iter->second.begin(), iter->second.end());
         iter->second.erase(unique(iter->second.begin(), iter->second.end()), iter->second.end());
      }
      else {
         info[pointer].push_back(pointee);
      }
   }

   static bool equals(MayPointToInfo *info1, MayPointToInfo *info2) {
       auto lhs = info1->info;
       auto rhs = info2->info;

       return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
   }


   static MayPointToInfo* join(MayPointToInfo *info1, MayPointToInfo *info2, MayPointToInfo *result) {
      if(result == NULL || info1 == NULL || info2 == NULL) return result;

      result->info.insert(info1->info.begin(), info1->info.end());
      for(auto val : info2->info) {
         for(auto pointee : val.second) {
            result->addInfo(val.first, pointee);
         }
      }

      return result;
   }   


};

#define ALLOCA        1
#define BITCAST       2
#define GETELEMENTPTR 3
#define LOAD          4
#define STORE         5
#define SELECT        6
#define PHI           7
#define OTHER         8

template <class Info, bool Direction>
class MayPointToAnalysis : public DataFlowAnalysis<Info, Direction> {

   private:

      void initializeForwardMap(Function * func) {
         DataFlowAnalysis<Info, Direction>::initializeForwardMap(func);
      }

      void initializeBackwardMap(Function * func) {
      }

      unsigned getInstrType(Instruction *I) {

         if(strcmp(I->getOpcodeName(), "alloca") == 0)
            return ALLOCA;

         if(strcmp(I->getOpcodeName(), "bitcast") == 0)
            return BITCAST;
         
         if(strcmp(I->getOpcodeName(), "getelementptr") == 0)
            return GETELEMENTPTR;

         if(strcmp(I->getOpcodeName(), "load") == 0)
            return LOAD;

         if(strcmp(I->getOpcodeName(), "store") == 0)
            return STORE;

         if(strcmp(I->getOpcodeName(), "select") == 0)
            return SELECT;

         if(strcmp(I->getOpcodeName(), "phi") == 0)
            return PHI;

         return OTHER;

      }

      bool isNotPointerOrStore(Instruction *I) {
         return !I->getType()->isPointerTy() && strcmp(I->getOpcodeName(), "store");
      }

      void flowfunction(Instruction * I,
                        std::vector<unsigned> & IncomingEdges,
                        std::vector<unsigned> & OutgoingEdges,
                        std::vector<Info *> & Infos) {


        

         if(I == NULL) return;

         Info *newInfo = new Info();

         unsigned index = this->getInstrToIndex(I);

         for(auto i : IncomingEdges) {
            MayPointToInfo * oldInfo = this->getEdgeToInfo(make_pair(i, index));
            MayPointToInfo::join(newInfo, oldInfo, newInfo);
         }

        
         if(isNotPointerOrStore(I)) { 
            for(unsigned i = 0; i < OutgoingEdges.size(); i++) {
               Infos.push_back(newInfo);
            }
            return;
         }

         unsigned instrType = getInstrType(I);
         switch(instrType) {
            case ALLOCA:   
               newInfo->addInfo(make_pair('R', index), make_pair('M', index));
               break;

            case BITCAST:
            case GETELEMENTPTR: {
                  unsigned i = this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(0)));
                  if(newInfo->info.find(make_pair('R', i)) != newInfo->info.end()) {
                     for(auto x : newInfo->info[make_pair('R', i)]) {
                        newInfo->addInfo(make_pair('R', index), x);
                     }
                  }
               }
               break;

            case LOAD: {
                  pointerInfo_t Rp = make_pair('R', this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(0))));
                  if(newInfo->info.find(Rp) != newInfo->info.end()) {
                     for(auto x : newInfo->info[Rp]) {
                        if(newInfo->info.find(x) != newInfo->info.end()) {
                           for(auto y : newInfo->info[x]) {
                              newInfo->addInfo(make_pair('R', index), y);
                           }
                        }
                     }
                  }
               }
               break;

            case STORE: {
                  pointerInfo_t Rv = make_pair('R', this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(0))));
                  pointerInfo_t Rp = make_pair('R', this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(1))));
                  if(newInfo->info.find(Rv) != newInfo->info.end()) {
                     for(auto x : newInfo->info[Rv]) {
                        if(newInfo->info.find(Rp) != newInfo->info.end()) {
                           for(auto y : newInfo->info[Rp]) {
                              newInfo->addInfo(y, x);
                           }
                        }
                     }
                  }
               }
               break;

            case SELECT: {
                  pointerInfo_t Ri = make_pair('R', index);
                  pointerInfo_t Rt = make_pair('R', this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(1))));
                  pointerInfo_t Rf = make_pair('R', this->getInstrToIndex(dyn_cast<Instruction>(I->getOperand(2))));
                  if(newInfo->info.find(Rt) != newInfo->info.end()) {
                     for(auto x : newInfo->info[Rt]) {
                        newInfo->addInfo(Ri, x);

                     }
                  }
                  if(newInfo->info.find(Rf) != newInfo->info.end()) {
                     for(auto x : newInfo->info[Rf]) {
                        newInfo->addInfo(Ri, x);
                     }
                  }
               }
               break;

            case PHI: {
                  pointerInfo_t Ri = make_pair('R', index);
                  for(auto ib = I->getParent()->begin(), ie = I->getParent()->end(); ib != ie; ib++) {
                     if(isa<PHINode>(&*ib)) {
                        PHINode *pn = llvm::dyn_cast<PHINode>(&*ib);   
                        for(unsigned ii = 0; ii < pn->getNumIncomingValues(); ii++) {
                           Instruction *instr = dyn_cast<Instruction>(pn->getOperand(ii));

                           if(instr != NULL) {
                              if(newInfo->info.find(make_pair('R', this->getInstrToIndex(instr))) != newInfo->info.end()) {
                                 for(auto x : newInfo->info[make_pair('R', this->getInstrToIndex(instr))]) {
                                    newInfo->addInfo(Ri, x);
                                 }
                              }
                           }
                        } // end for()
                     }
                  } // end for()

               }
               break;

            default:
               break;

         }


         for(unsigned i = 0; i < OutgoingEdges.size(); i++) {
            Infos.push_back(newInfo);
         }

      } // end flowfunction

   public:
      MayPointToAnalysis(Info &bottom, Info &initState) : DataFlowAnalysis<Info, Direction>(bottom, initState) {}

};


namespace {
   struct MayPointToAnalysisPass : public FunctionPass {
      public:
         static char ID;
         MayPointToAnalysisPass() : FunctionPass(ID) {}
         bool runOnFunction(Function &F) override { 
            MayPointToInfo *bott = new MayPointToInfo();
            MayPointToInfo *init = new MayPointToInfo();

            MayPointToAnalysis<MayPointToInfo, true> analysis(*bott, *init);

            analysis.runWorklistAlgorithm(&F);
            analysis.print();
            return false;
         }
   }; 
}

char MayPointToAnalysisPass::ID = 0;
static RegisterPass<MayPointToAnalysisPass> X("cse231-maypointto",
                                            "Developed to test LLVM and docker",
                                            false /* Only looks at CFG */,
                                            false /* Analysis Pass */);