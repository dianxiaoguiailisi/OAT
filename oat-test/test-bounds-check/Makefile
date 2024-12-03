# 指定了 Clang 编译器的路径
CLANG = ~/work/llvm-env/build/bin/clang
# 指定了 LLVM opt 工具的路径
OPT = ~/work/llvm-env/build/bin/opt
# 为 TEST 变量设置一个默认值，如果没有在其他地方指定 TEST 的值，那么它将默认为 test_combo
TEST ?= test_combo
# 交叉编译器 aarch64-linux-gnu-gcc，用于生成 AArch64 架构的代码
CC = aarch64-linux-gnu-gcc

# TEE部分..........

# 指定了 LLVMNova.so 所在路径
NOVA_PATH = ~/work/llvm-env/build/lib/
# 指定了 llc（LLVM 目标代码生成器）的路径，用于将 LLVM IR 转换为目标平台汇编。
LLC_ARM = ~/work/llvm-env/build/bin/llc
# 指定了 llvm-dis 工具的路径，用于将 LLVM IR（二进制形式）转换回文本形式
DIS = ~/work/llvm-env/build/bin/llvm-dis
# 指定了运行时路径，可能用于链接时的运行时库
RT = /path/to/work/ra-project/data/




# 使用 clang 将 test.c 源代码编译成 LLVM 位码
test:
	$(CLANG) -S -emit-llvm test.c -o test.bc

# 使用 llvm-link 工具将 test.bc 文件链接为一个组合的 LLVM IR 文件
test-combo: test 
	llvm-link test.bc -o test_combo.bc

opt-test: test-combo
# 使用 OPT 工具加载 LLVMNova.so 插件并应用 -nova 优化（可能是一些特定的优化或者插桩）
	$(OPT) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 
# 加载 LLVMCollectCFVHints.so 插件，并应用三个提示收集传递（collect-ibranch-hints-pass、collect-icall-hints-pass 和 collect-loop-hints-pass），收集控制流分支、函数调用和循环等相关提示信息
	$(OPT) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
#生成汇编代码
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s
# 生成 LLVM IR 的文本格式
	$(DIS) combo.bc -o combo.ll
#生成可执行文件
	$(CLANG) combo.bc -L$(RT)/runtime -lm -lrt -lnova -lsoftboundcets_rt
