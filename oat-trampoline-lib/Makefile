# OP-TEE开发环境路径
TEE_PATH ?= /home/yourusername/work/tee-workspace/devel/optee-hikey-debian/
# 交叉编译工具链的路径
TOOLCHAIN ?= $(TEE_PATH)/toolchains/aarch64/bin/
#Trusted Application 的路径
TA ?= $(TEE_PATH)/hello_world/ta
# OP-TEE 客户端库的路径。
TEEC_EXPORT ?= $(TEE_PATH)/optee_client/out/export
# 编译时的标志，包含对头文件的路径
CFLAGS += -Wall -I$(TA)/include -I$(TEEC_EXPORT)/include -I./include
LDADD += -lteec -L$(TEEC_EXPORT)/lib
# 选择的 C 编译器，这里使用 clang
CC = clang
/指定目标架构为 aarch64-unknown-linux。
TARGET = -target aarch64-unknown-linux
# 交叉编译工具链的 GCC 编译器路径
CCC = $(TOOLCHAIN)/aarch64-linux-gnu-gcc
# softboundcets 项目特定的编译标志
SOFT_CFLAGS=-Wall -pedantic  -O3 -D__SOFTBOUNDCETS_SPATIAL
# 用于创建静态库时的 ar 工具标志
ARFLAGS=-rcs 
# 编译 nova.c 文件生成目标文件 nova.o，使用的是交叉编译器 $(CCC)
nova:
	#$(CC) $(TARGET) -fPIC -g -c -Wall nova.c -o nova.o
	$(CCC) -fPIC -g -c -Wall nova.c -o nova.o
# 编译 cfv_bellman.c 文件生成目标文件 cfv_bellman.o，并链接了之前定义的 CFLAGS（包含路径）。
cfv-bellman:
	#$(CC) $(TARGET) -fPIC -g -c $(CFLAGS) -Wall cfv_bellman.c -o cfv_bellman.o
	$(CCC) -fPIC -g -c $(CFLAGS) -Wall cfv_bellman.c -o cfv_bellman.o
# 编译汇编文件 trampoline.S 生成目标文件 trampoline.o。
trampoline:
	#$(CC) $(TARGET) -fPIC -g -c -Wall trampoline.S -o trampoline.o
	$(CCC) -fPIC -g -c -Wall trampoline.S -o trampoline.o
# 将之前生成的目标文件 nova.o、trampoline.o 和 cfv_bellman.o 链接成一个共享库 libnova.so.1.0.1，并指定 libnova.so.1 的 soname。链接时使用了 LDADD 中定义的库。
lib-a64: nova cfv-bellman trampoline
	$(CCC) $(LDADD) -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o trampoline.o cfv_bellman.o -lc

lib-aarch64:
	clang  -fPIC -g -c -Wall nova.c
	clang  -fPIC -g -c -Wall cfv_bellman.c
	clang  -fPIC -g -c -Wall trampoline.S
	clang -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o trampoline.o cfv_bellman.o -lc
	sudo cp libnova.so.1.0.1 /usr/lib/
	sudo ln -fs libnova.so.1.0.1 /usr/lib/libnova.so.1
	sudo ln -fs /usr/lib/libnova.so.1 /usr/lib/libnova.so

softboundcets_rt: softboundcets.h softboundcets-checks.c softboundcets.c softboundcets-wrappers.c
	clang $(SOFT_CFLAGS) -c softboundcets-checks.c -o softboundcets-checks.o
	clang $(SOFT_CFLAGS) -c softboundcets.c -o softboundcets.o
	clang $(SOFT_CFLAGS) -c softboundcets-wrappers.c -o softboundcets-wrappers.o
	ar $(ARFLAGS) libsoftboundcets_rt.a softboundcets.o softboundcets-checks.o softboundcets-wrappers.o

lib-x64:
	clang  -fPIC -g -c -Wall nova.c
	clang -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o -lc
	sudo cp libnova.so.1.0.1 /usr/lib/
	sudo ln -fs libnova.so.1.0.1 /usr/lib/libnova.so.1
	sudo ln -fs /usr/lib/libnova.so.1 /usr/lib/libnova.so

sync:
	cp Makefile ~/work/ra-project/data/runtime/
	cp nova.c ~/work/ra-project/data/runtime/
	cp trampoline.S ~/work/ra-project/data/runtime/

send:
	scp libnova.so.1.0.1  your-pc:~/tmp/hikey-relay/

bin-microbench:
	clang microbench.c -lnova -lteec -o microbench
