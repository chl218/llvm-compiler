#include <vector>
#include <map>
#include <string>

#include "llvm/Pass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/GlobalVariable.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;


namespace {
   struct TestPass : public FunctionPass {
      
      private:
         map<uint32_t, uint32_t> instrCounter;

      public:   
         static char ID;

         TestPass() : FunctionPass(ID) {}

         bool runOnFunction(Function &F) override {

            //------------------------------------------------------------------
            // Section 3: Profiling Branch Bias
            //------------------------------------------------------------------

            // Get the function to call from our runtime library.
            LLVMContext &ctx   = F.getContext();
            Constant *libPrint = F.getParent()->getOrInsertFunction(
                                                   "printOutBranchInfo",
                                                   Type::getVoidTy(ctx),
                                                   nullptr
                                                   );

            Constant *libUpdate = F.getParent()->getOrInsertFunction(
                                                   "updateBranchInfo",      
                                                   Type::getVoidTy(ctx),   
                                                   Type::getInt1Ty(ctx),
                                                   nullptr
                                                   );

            Function *callPrint  = cast<Function>(libPrint);
            Function *callUpdate = cast<Function>(libUpdate);

            for (auto& B : F) {
               instrCounter.clear();
               for(auto& I : B) {

                  IRBuilder<> builder(B.getTerminator());
                  
                  // is branch instruction
                  if(BranchInst::classof(&I)) {
                     // cast into branch instruction
                     if(auto* op = dyn_cast<BranchInst>(&I)) {
                        // is conditional branch e.g. if/else etc
                        if(op->isConditional()) {
                           // is branch taken
                           Value *boolArg = op->getCondition();

                           vector<Value*> putArgs;
                           putArgs.push_back(boolArg);
                           ArrayRef<Value *> argsRef(putArgs);

                           builder.CreateCall(callUpdate, putArgs);
                        } // end if-branch-conditional
                     } // end if-branch-instr
                  } // end if-branch-class

                  if((string)I.getOpcodeName() == "ret") {
                     builder.CreateCall(callPrint);
                  }

               } // end instr-block
            } // end func-block

            return true;

         } // end runOnFunction(...)


   }; // end TestPass
} // end namespace
         

char TestPass::ID = 3;
static RegisterPass<TestPass> X("cse231-bb",
                                "Developed to test LLVM and docker",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);
