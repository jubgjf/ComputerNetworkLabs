#pragma once

#include <stdlib.h>
#include <time.h>

// 发送方报文格式：
//
// 报文前 4 个字节为 'S', 'E', 'N', 'D'，
// 第 5 - 8 个字节为序号，序号为 unsigned 类型，
// 其余部分为正文
//
// 示例如下：
//
// 01010011 01000101 01001110 01000100 -> SEND
// 00000000 00000000 00000000 00000101 -> 序号为 5 号
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 实际数据
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx

// 接收方响应报文格式：
//
// 报文前 4 个字节为 'A', 'C', 'K', 'K'，
// 第 5 - 8 个字节为序号，序号为 unsigned 类型
// 其余部分为正文
//
// 示例如下：
//
// 01000001 01000011 01001011 01001011 -> ACKK
// 00000000 00000000 00000000 00000101 -> 序号为 5 号
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx -> 实际数据
// xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx

/**
 * @brief 服务器端口号
 */
#define SERVER_PORT 10000

/**
 * @brief 获取当前时间字符串，格式为 %H:%M:%S
 *
 * @return char* 返回格式化的字符串
 */
char* get_time();
