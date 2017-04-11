#include <map>
#include <string>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"

using namespace llvm;

namespace {
   struct TestPass : public FunctionPass {

      static char ID;

      std::map<std::string, int> cntr;

      TestPass() : FunctionPass(ID) {}

      bool runOnFunction(Function &F) override {
         //--------------------------------------------------------------------
         // Section 1: Collecting Static Instruction Counts
         //--------------------------------------------------------------------

         // clear hash map
         cntr.clear();

         // for each function, iterate through each instruction and keep count
         // of the individual instructions encountered
         for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
            // get instruction's string name
            std::string instr = i->getOpcodeName();
            cntr[instr]++;
         } // end for

         // print result
         for (std::map<std::string, int>::iterator i = cntr.begin(), e = cntr.end(); i != e; ++i) {
            errs() << i->first << '\t' << i->second << '\n';
         }

         return false;  // no modification to the code
      } // end runOnFunction(...)


   }; // end TestPass
} // end namespace


char TestPass::ID = 1;
static RegisterPass<TestPass> X("cse231-csi", "Developed to test LLVM and docker",
                                 false /* Only looks at CFG */,
                                 false /* Analysis Pass */);
