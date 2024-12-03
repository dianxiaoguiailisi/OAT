# 指定测试的文件名
TEST ?= test_combo
# 设置为AArch64编译器
CC = aarch64-linux-gnu-gcc
# NUC_PC和hikey定义目标主机的SSH地址
NUC_PC ?= name@ip
objdump ?= aarch64-linux-gnu-objdump 
hikey ?= linaro@ip
# 指定OP-TEE客户端的导出路径
TEEC_EXPORT ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/optee_client/out/export
# 定义OP-TEE应用的路径
TA ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/hello_world/ta

# 定义了链接器选项，将 OP-TEE 客户端库路径添加到链接器的库路径中 (-L$(TEEC_EXPORT)/lib)，并链接 -lteec 库。
LDADD += -lteec -L$(TEEC_EXPORT)/lib
# 路径定义
# NOVA_PATH：LLVM Nova插件的库路径
NOVA_PATH = ~/work/llvm-env/build/lib/
# LLC_ARM：LLVM中用于生成目标架构汇编代码的工具路径
LLC_ARM = ~/work/llvm-env/build/bin/llc
# LLVM工具集的路径
LLVM_PATH = ~/work/llvm-env/build/bin/

# 规则
# 1.将生成的汇编文件 combo.s 上传到远程主机 pc 的 /tmp/hikey-relay/ 目录
send-combo-s:
	scp combo.s pc:~/tmp/hikey-relay/
# 2. 使用 llc 工具将 LLVM 位码文件 test_combo.bc 转换为 AArch64 架构的汇编代码 combo.s，并禁用 aarch64-enable-cfv 选项。
asm-combo:
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv=false test_combo.bc -o combo.s
# 3.使用 opt 工具和 Nova 插件对 hello.bc 进行优化（-nova），并生成 combo.bc
opt-hello:
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < hello.bc > combo.bc 2>/dev/null
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s
# 4.先构建 vf-combo，然后使用 opt 工具加载 LLVMCollectCFVHints.so 插件，收集条件分支的控制流提示，并生成 combo_hints.bc。
local-test: vf-combo
	#$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc
	#$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < vf_combo.bc > combo_hints.bc
	$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-cond-branch-hints-pass < vf_combo.bc > combo_hints.bc
# 使用 llvm-dis 将位码文件 combo_hints.bc 反汇编为人类可读的汇编文件 combo_hints.dis。
dis-combo:
	llvm-dis < combo_hints.bc >combo_hints.dis

# 使用 clang++ 编译 vf.cpp 源代码，生成对应的 LLVM 位码文件 vf.bc。
vf:
	$(LLVM_PATH)/clang++ -S -emit-llvm vf.cpp -o vf.bc
# 使用 llvm-link 将 vf.bc 链接到一起生成 vf_combo.bc，为后续的优化和控制流验证准备位码文件
vf-combo: vf
	$(LLVM_PATH)/llvm-link vf.bc -o vf_combo.bc
# 
opt-vf: vf-combo
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < vf_combo.bc > combo.bc 
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s
