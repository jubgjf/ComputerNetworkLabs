#pragma once

/**
 * @brief 发送窗口大小
 */
#define SENDER_WINDOW_SIZE 3

/**
 * @brief 发送的报文内容及其在窗口中的序号
 */
struct message_seg {
    char     msg[100]; // 待发送的内容
    unsigned index;    // 待发送内容在发送窗口中的序号
};

/**
 * @brief 发送一个报文的线程
 *
 * @param msg
 */
void threaded_client(void* seg);
