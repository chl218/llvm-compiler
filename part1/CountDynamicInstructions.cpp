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
            // Section 2: Collecting Dynamic Instruction Counts
            //------------------------------------------------------------------

            // Get the function to call from our runtime library.

            // getOrInsertFunction(external function name,
            //                     function return type,
            //                     parameter type,
            //                     ...
            //                     nullptr denoting end of parameter types)
            LLVMContext &ctx   = F.getContext();
            Constant *libPrint = F.getParent()->getOrInsertFunction(
                                                   "printOutInstrInfo",
                                                   Type::getVoidTy(ctx),
                                                   nullptr
                                                   );

            Constant *libUpdate = F.getParent()->getOrInsertFunction(
                                                   "updateInstrInfo",      
                                                   Type::getVoidTy(ctx),   
                                                   Type::getInt32Ty(ctx),  
                                                   Type::getInt32PtrTy(ctx),
                                                   Type::getInt32PtrTy(ctx),
                                                   nullptr
                                                   );

            Function *callPrint  = cast<Function>(libPrint);
            Function *callUpdate = cast<Function>(libUpdate);

            bool printFlag = false;

            // for each function block
            for (auto& B : F) {
               // keep instruction counter per block
               instrCounter.clear();
               // for each block
               for(auto& I : B) {
                  // record block instruction count
                  instrCounter[I.getOpcode()]++;

                  // if at end of block, print dynamic counts
                  //errs() << I.getOpcodeName() << '\n';
                  if((string)I.getOpcodeName() == "ret") {
                     printFlag = true;
                  }
               }

               // push key-value pairs for external lib function
               unsigned sizeArr = instrCounter.size();
               vector<uint32_t> keyArr;
               vector<uint32_t> valArr;               
               for(auto& kv : instrCounter) {
                  keyArr.push_back(kv.first);
                  valArr.push_back(kv.second);
               }

               // array pointer type
               ArrayType *arrType = ArrayType::get(IntegerType::getInt32Ty(ctx), sizeArr);
               
               // constant* wrapper for GlobalVariable 
               Constant  *keyType = ConstantDataArray::get(ctx, keyArr);
               Constant  *valType = ConstantDataArray::get(ctx, valArr);   

               // GlobalVariabl* wrapper for array pointer
               GlobalVariable *keyPtr = new GlobalVariable(*(F.getParent()), arrType, true, GlobalValue::ExternalLinkage, 0, "keyptr");
               GlobalVariable *valPtr = new GlobalVariable(*(F.getParent()), arrType, true, GlobalValue::ExternalLinkage, 0, "valPtr");

               keyPtr->setInitializer(keyType);
               valPtr->setInitializer(valType);

               // for ArrayRef<Constant*>
               ConstantInt *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);

               Constant *numArg = ConstantInt::get(Type::getInt32Ty(ctx), sizeArr);
               Constant *keyArg = ConstantExpr::getInBoundsGetElementPtr(arrType, keyPtr, ArrayRef<Constant*>{zero, zero});
               Constant *valArg = ConstantExpr::getInBoundsGetElementPtr(arrType, valPtr, ArrayRef<Constant*>{zero, zero});

               vector<Value*> putArgs;
               putArgs.push_back(numArg);
               putArgs.push_back(keyArg);
               putArgs.push_back(valArg);
               ArrayRef<Value *> argsRef(putArgs);

               IRBuilder<> builder(B.getTerminator());

               builder.SetInsertPoint(dyn_cast<Instruction>(B.begin()));
               builder.CreateCall(callUpdate, argsRef);
               //builder.CreateCall(callUpdate, {numArg, keyArg, valArg});

               if(printFlag) {
                  builder.CreateCall(callPrint);
                  printFlag = false;
               }

            }

            return true;

         } // end runOnFunction(...)


   }; // end TestPass
} // end namespace
         

char TestPass::ID = 2;
static RegisterPass<TestPass> X("cse231-cdi",
                                "Developed to test LLVM and docker",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);
