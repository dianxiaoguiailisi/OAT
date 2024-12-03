#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "nova.h"
#include "cfv_bellman.h"

//#define DEBUG_NOVA
//#define TEST_BOUNDS_CHECK

/*定义了一个条件编译的调试输出宏 debug_info。
        当定义了 DEBUG_NOVA 时，debug_info 宏会输出调试信息（使用 printf）。
        否则，宏定义为空，不做任何输出。*/
#ifdef DEBUG_NOVA
#define debug_info(...)     \
    do { printf(__VA_ARGS__);} while (0)
#else
#define debug_info(...)     \
    do { } while (0)
#endif

/* hints related */
extern FILE *hfp;//文件指针
extern bool cfv_start;//是否开始收集事件

void cfv_icall(uint64_t target, uint64_t pc);
void cfv_ijmp(uint64_t target, uint64_t pc);
void cfv_ret(uint64_t target, uint64_t pc);
// 数据事件处理函数
void __record_defevt(uint64_t addr, uint64_t val) {
    debug_info("%s addr: %lx val: %lx\n", __func__, addr, val);
    handle_event(CFV_EVENT_DATA_DEF, addr, val);
}

void __check_useevt(uint64_t addr, uint64_t val) {
    debug_info("%s addr: %lx val: %lx\n", __func__, addr, val);
    handle_event(CFV_EVENT_DATA_USE, addr, val);
}
//控制流事件收集
void __collect_cond_branch_hints(bool cond) {
    if (cond)
        handle_event(CFV_EVENT_HINT_CONDBR, 1, 0);
    else
        handle_event(CFV_EVENT_HINT_CONDBR, 0, 0);

    if (hfp == NULL || cfv_start == false)
	return;
    if (cond)
        fprintf(hfp, "y");
    else
        fprintf(hfp, "n");
}

void __collect_icall_hints(uint64_t fid, uint64_t count, uint64_t func) {
    debug_info("%s fid: 0x%lx count: 0x%lx funcaddr: 0x%lx\n", __func__, fid, count, func);
}

void __collect_ibranch_hints(uint64_t fid, uint64_t count, uint64_t target) {
    debug_info("%s fid: 0x%lx count: 0x%lx funcaddr: 0x%lx\n", __func__, fid, count, target);
}
// 间接调用、跳转和返回的处理
void cfv_icall(uint64_t target, uint64_t pc) {
    debug_info("%s dest: %lx src: %lx\n", __func__, target, pc);
    handle_event(CFV_EVENT_HINT_ICALL, pc, target);
    if (hfp == NULL||cfv_start == false)
	return;
    fprintf(hfp,"%s dest: %lx src: %lx\n", __func__, target, pc);
}

void cfv_ret(uint64_t target, uint64_t pc) {
    debug_info("%s dest: %lx src: %lx\n", __func__, target, pc);
    handle_event(CFV_EVENT_CTRL, pc, target);
}

void cfv_ijmp(uint64_t target, uint64_t pc) {
    debug_info("%s dest: %lx src: %lx\n", __func__, target, pc);
    handle_event(CFV_EVENT_HINT_IBR, pc, target);
    if (hfp == NULL||cfv_start == false)
	return;
    fprintf(hfp,"%s dest: %lx src: %lx\n", __func__, target, pc);
}

