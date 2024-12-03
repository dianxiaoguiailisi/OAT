#ifndef CFV_BELLMAN_H_
#define CFV_BELLMAN_H_

#include <stddef.h>
#include <stdint.h>

/* CFV event types */
#define CFV_EVENT_CTRL		0x00000010
#define CFV_EVENT_DATA_DEF	0x00000020
#define CFV_EVENT_DATA_USE	0x00000040
#define CFV_EVENT_HINT_CONDBR	0x00000080
#define CFV_EVENT_HINT_ICALL	0x00000100
#define CFV_EVENT_HINT_IBR  	0x00000200

/* 正常世界 API */

uint32_t cfv_init(void);//初始化CFV模块，打开TA，设置初始状态，并准备事件文件。
uint32_t cfv_quote(void);//获取并打印CFV收集的事件数据，关闭TA
uint32_t handle_event(uint64_t event_type, uint64_t a, uint64_t b);//发送事件数据到TA进行验证


#endif /* CFV_BELLMAN_H*/
