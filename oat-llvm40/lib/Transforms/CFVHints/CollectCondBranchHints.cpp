//==- CollectCondBranchHints.cpp - 为控制流验证收集条件分支提示 ------------------===//
// 该 Pass 与 AArch64 后端的控制流验证（Control-Flow Verification） Pass 协作。
// 为了提供提示信息，使得验证器能够快速验证控制流的哈希值，我们需要为
// 每个条件分支收集提示信息，使用 1 位来记录基本的“已采取”或“未采取”信息。
//===----------------------------------------------------------------------===//
#include "llvm/Pass.h"
#include <string>
#include <vector>
#include <fstream>
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Mangler.h" 
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

// 宏定义部分
#define DEBUG_TYPE "collect-cond-branch-hints"
#define INSTRUMENT_ALL
STATISTIC(NumOfCondBranches, "Number of conditional branch inst");//统计 被插桩的条件分支指令的数量，并在最终的编译输出中显示这个统计信息
using namespace llvm;

typedef std::map<std::string, int> FunctionMap;
//使用了 LLVM 提供的 cl::opt 类来处理命令行选项（命令行参数）
  //CollectCondBranchHintsInfo 用来控制是否收集条件分支的提示信息。如果命令行没有传递该选项，则默认为 true，即会收集条件分支的提示信息。如果传递了 -collect-cond-branch-hints=false，则该功能会被禁用
static cl::opt<bool> CollectCondBranchHintsInfo("collect-cond-branch-hints", cl::Hidden,cl::init(true));
  //InstrumentFunctionNameListFile：指定包含需要插桩的函数名称的文件路径，默认值为 funclist.txt，可以通过 -funclist 来指定不同的文件
static cl::opt<std::string> InstrumentFunctionNameListFile("funclist", cl::Hidden, cl::desc("Specify input file that list the function names that need to be instrumented"),cl::init("funclist.txt"));


namespace {
    struct  CollectCondBranchHints : public FunctionPass {
      static char ID;
      static FunctionMap *fMap;
      static int FID;
      static std::vector<std::string> *list;
      bool Modified = false;
      /*构造函数*/
      CollectCondBranchHints();
      /*LLVM Pass核心机制：runOnFunction函数会被调用一次，对每个传入的 Function 执行相应的操作*/
      bool runOnFunction(Function &F) override ;
      /*插桩函数instrumentCondBranch：I：有条件判断指令;cond:条件表达式*/
      bool instrumentCondBranch(Instruction *I, Value *cond);
    };
  }

  CollectCondBranchHints::CollectCondBranchHints() : FunctionPass(ID) {
      list->clear();
      std::ifstream infile(InstrumentFunctionNameListFile);//从InstrumentFunctionNameListFile中读取数据
      std::string word;
      //读取文件内容:函数名称
      while(infile>>word) {
          list->push_back(word);
      }
      if( list->empty()) {
          errs() << "Note: funclist is empty! use -funclist filename to designate funclist file" << "\n";
          }
    }
  bool CollectCondBranchHints::runOnFunction(Function &F)  {
          std::string name = F.getName().str();//获取函数名字
          // 插桩函数判断
          #ifdef INSTRUMENT_ALL // 如果宏 INSTRUMENT_ALL 被定义，则会对所有函数进行插桩
          #else // 根据函数名列表（InstrumentFunctionNameListFile文件中），决定是否对当前函数进行插桩。
              errs() << __func__ << " : "<< name << "\n";
              if (std::find(std::begin(*list), std::end(*list), name) == std::end(*list)) { //如果函数名称 name 不在函数列表里，则跳过该函数，输出信息并返回 false。
                  errs() << __func__ << ": skip function "<< name << ", for it is not in list"<<"\n";
                  return false;
              }
          #endif
          //每个函数独一ID编号
          if (fMap->find(name) != fMap->end()) {
                (*fMap)[name] = ++FID; 
          }
          //核心：遍历函数中的每个基本块中每一个指令，并对有条件分支进行插桩
          for (BasicBlock &BB : F) {
              for  (Instruction &I : BB) {//每一个指令
                  switch (I.getOpcode()) {//根据指令的操作码（opcode）来判断指令的类型
                      case Instruction::Br: {//分支指令
                          BranchInst *bi = cast<BranchInst>(&I);
                          if (bi->isUnconditional()) {//无条件分支直接跳过
                          } else {//有条件分支
                              //errs() <<"CondBranch: " << *bi << "\n";
                              Modified |= instrumentCondBranch(&I, bi->getCondition());//插桩【 bi->getCondition()判断条件表达式】
                              NumOfCondBranches++;
                          }
                          break;
                      }

                      default:
                        //errs() << I << "\n";
                        break;
                  }
              }	
          }

          return Modified;
  }
  bool CollectCondBranchHints::instrumentCondBranch(Instruction *I, Value *cond) {
    IRBuilder<> B(I);//创建一个 IRBuilder 对象，用来在给定的指令 I 后插入新的指令
    Module *M = B.GetInsertBlock()->getModule();//获得基本块所属的模块【所有的函数、全局变量等都会被包含在其中；向模块中插入新的函数或修改现有函数需要访问模块】
    Type *VoidTy = B.getVoidTy();
    Type *I1Ty = B.getInt1Ty();

    //errs() << __func__ << " : "<< *I<< "\n";
    //向 Module 中插入一个新的函数声明
    Constant *ConstCollectCondBranchHints= M->getOrInsertFunction("__collect_cond_branch_hints"/*插入函数名字*/, VoidTy/*返回类型*/,I1Ty/*函数参数*/, nullptr/*参数列表结束*/);
    //类型转换：常量类型转换为函数类型
    Function *FuncCollectCondBranchHints= cast<Function>(ConstCollectCondBranchHints);
    //插入调用指令
    B.CreateCall(FuncCollectCondBranchHints/*目的地址*/, {cond}/*参数*/);

    return true;

}
//CollectCondBranchHints 类的静态成员变量初始化
char CollectCondBranchHints::ID = 0;
int CollectCondBranchHints::FID = 0;
FunctionMap *CollectCondBranchHints::fMap= new FunctionMap();
std::vector<std::string> *CollectCondBranchHints::list = new std::vector<std::string>();
//将 CollectCondBranchHints 类注册到 LLVM 的 Pass 管理系统中，使其成为一个可用的 Pass
static RegisterPass<CollectCondBranchHints> X("collect-cond-branch-hints-pass", "Collect Conditional Branch Hints Info", false, false);