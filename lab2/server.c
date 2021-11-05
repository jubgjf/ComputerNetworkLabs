#include "server.h"
#include "common.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef SR
/**
 * @brief 接收方窗口
 */
int sender_window[1024] = {0};
#endif

int main() {
#ifdef SR
    printf("[info %s] Running in SR!\n", get_time());
#else
    printf("[info %s] Running in GBN!\n", get_time());
#endif

    // 监听客户的套接字
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("[error] socket");
        return -1;
    }

    // 防止出现 bind: Address already in use
    int on = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) ==
        -1) {
        perror("[error] setsockopt reuseaddr");
        return -1;
    }

    // 绑定本地地址
    struct sockaddr_in server_saddr;
    memset(&server_saddr, 0, sizeof(server_saddr));
    server_saddr.sin_family      = AF_INET;
    server_saddr.sin_addr.s_addr = INADDR_ANY;
    server_saddr.sin_port        = htons(SERVER_PORT);
    if (bind(server_fd, (struct sockaddr*)&server_saddr,
             sizeof(server_saddr)) == -1) {
        perror("[error] bind");
        return -1;
    }

    // 最后一个成功接收的分组
    unsigned excepted_index = 0;

#ifdef LOSS
    // 模拟的数据包丢失
    int loss[1024] = {0};
    loss[0]        = 1; // 0 号丢失 1 次
    loss[3]        = 1; // 3 号丢失 1 次
    loss[4]        = 2; // 4 号丢失 2 次
#endif

    // 接收客户消息
    while (1) {
        char               recv_buf[1024] = {0};
        struct sockaddr_in client_addr;
        socklen_t          len = sizeof(client_addr);

        // 接收客户消息
        if (recvfrom(server_fd, recv_buf, sizeof(recv_buf), 0,
                     (struct sockaddr*)&client_addr, &len) == -1) {
            perror("[error] recvfrom");
            return -1;
        }

        // 检查 0 - 3 字节是否为 "SEND"
        char first4[5] = {0};
        strncpy(first4, recv_buf, sizeof(first4));
        if (strcmp(first4, "SEND")) {
            printf("[error %s] first 4 != \"SEND\"\n", get_time());
            return -1;
        }

        // 检查 4 - 7 字节的序号
        unsigned index = 0;
        index |= recv_buf[4] << 24;
        index |= recv_buf[5] << 16;
        index |= recv_buf[6] << 8;
        index |= recv_buf[7] << 0;

#ifdef LOSS
        // 模拟的数据包丢失
        if (loss[index]) {
            printf("[info %s] Make index %u packet loss!\n", get_time(), index);
            loss[index]--;
            continue;
        }
#endif

        // 有效信息
        printf("%27s+--------------------------------\n", " ");
        printf("[info %s] Client ==> | SEND %u %s\n", get_time(), index,
               recv_buf + 8);
        printf("%27s+--------------------------------\n", " ");

#ifdef SR
        sender_window[index] = 1;
#endif

        // 检查接收到的分组是否是想要的
        if (excepted_index == index) {
            // 是想要的
            excepted_index++;
            printf("[info %s] Index %u accepted!\n", get_time(), index);
        } else {
#ifdef SR
            // 不是想要的，但是缓存
            printf("[info %s] Index %u cached!\n", get_time(), index);
#else
            // 不是想要的，不发 ACK
            printf("[info %s] Index %u not accept, waiting for index %u!\n",
                   get_time(), index, excepted_index);
            continue;
#endif
        }

        // 回复客户
        char send_buf[1024] = {0};

        // 0 - 3 字节为 ACKK
        strcat(send_buf, "ACKK");

        // 4 - 7 字节为序号
        send_buf[4] = (index >> 24) & 0xFF;
        send_buf[5] = (index >> 16) & 0xFF;
        send_buf[6] = (index >> 8) & 0xFF;
        send_buf[7] = (index >> 0) & 0xFF;

        // 正文部分填充时间
        strcpy(send_buf + 8, get_time());

        sendto(server_fd, send_buf, sizeof(send_buf), 0,
               (struct sockaddr*)&client_addr, len);
    }

    close(server_fd);

    return 0;
}
