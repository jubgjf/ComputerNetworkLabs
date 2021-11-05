#pragma once

#include <stdint.h>

/**
 * @brief 发送窗口大小
 */
#define SENDER_WINDOW_SIZE 3

/**
 * @brief 发送的报文内容及其在窗口中的序号
 */
struct message_seg_port {
    char     msg[100];           // 待发送的内容
    unsigned index;              // 待发送内容在发送窗口中的序号
    uint16_t local_server_port;  // 本地服务器端口号
    uint16_t remote_server_port; // 远程服务器端口号
};

/**
 * @brief 发送方主线程
 *
 * @param main_arg_port main 函数参数和端口号
 */
void client(void* main_arg_port);

/**
 * @brief 发送一个报文的线程
 *
 * @param seg 发送的报文信息
 */
void threaded_client(void* seg);
