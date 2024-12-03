/*
 * 版权所有 (c) 2016, Linaro 有限公司
 * 保留所有权利。
 *
 * 在符合以下条件的前提下，允许以源代码和二进制形式进行再分发和使用，无论是修改过还是未修改过：
 *
 * 1. 源代码的再分发必须保留上述版权声明、此条件列表和以下免责声明。
 *
 * 2. 二进制形式的再分发必须在分发的文档和/或其他材料中复制上述版权声明、此条件列表和以下免责声明。
 *
 * 本软件由版权持有人和贡献者“按原样”提供，不提供任何明示或暗示的担保，包括但不限于
 * 对适销性和特定用途适用性的默示担保。在任何情况下，版权持有人或贡献者均不对因使用本软件
 * 而导致的任何直接、间接、附带、特殊、示范性或后果性损害（包括但不限于购买替代商品或服务；
 * 使用、数据或利润的损失；或业务中断）承担责任，无论是基于合同、严格责任，还是侵权（包括过失或其他）
 * 理论，也不管是否已被告知发生此类损害的可能性。
 */

#ifndef TA_HELLO_WORLD_H
#define TA_HELLO_WORLD_H

/* This UUID is generated with uuidgen
   the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html */
#define TA_HELLO_WORLD_UUID { 0x8aaaf200, 0x2450, 0x11e4, \
		{ 0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b} }

/* The Trusted Application Function ID(s) implemented in this TA */
#define TA_HELLO_WORLD_CMD_INC_VALUE	0
#define TA_CMD_CFA_VERIFY_EVENTS	1
#define TA_CMD_CFA_INIT			2
#define TA_CMD_CFA_QUOTE		3
#define TA_CMD_CFA_SETUP		4

#endif /*TA_HELLO_WORLD_H*/
