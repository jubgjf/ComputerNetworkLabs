#include "server.h"
#include "common.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void server(void* main_arg_port) {
#ifdef SR
    printf("[info %s] Running in SR!\n", get_time());

    // 接收方窗口
    int      recv_window[1024] = {0};
    unsigned recv_window_size  = 3;
    unsigned recv_base         = 0;
    unsigned recv_next_seq     = recv_base + recv_window_size;
#else
    printf("[info %s] Running in GBN!\n", get_time());
#endif

    struct main_arg_port arg_p = *(struct main_arg_port*)main_arg_port;
    uint16_t             local_server_port = arg_p.local_server_port;

    // 监听客户的套接字
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("[error] socket");
        exit(-1);
    }

    // 防止出现 bind: Address already in use
    int on = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) ==
        -1) {
        perror("[error] setsockopt reuseaddr");
        exit(-1);
    }

    // 绑定本地地址
    struct sockaddr_in server_saddr;
    memset(&server_saddr, 0, sizeof(server_saddr));
    server_saddr.sin_family      = AF_INET;
    server_saddr.sin_addr.s_addr = INADDR_ANY;
    server_saddr.sin_port        = htons(local_server_port);
    if (bind(server_fd, (struct sockaddr*)&server_saddr,
             sizeof(server_saddr)) == -1) {
        perror("[error] bind");
        exit(-1);
    }

    // 最后一个成功接收的分组
    unsigned excepted_index = 0;

#ifdef LOSS
    // 模拟的数据包丢失
    int loss[1024] = {0};
    loss[0]        = 1; // 0 号丢失 1 次
    loss[3]        = 1; // 3 号丢失 1 次
    loss[4]        = 2; // 4 号丢失 2 次
    loss[6]        = 1; // 6 号丢失 1 次
    loss[7]        = 1; // 7 号丢失 1 次
    loss[9]        = 2; // 9 号丢失 2 次
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
            exit(-1);
        }

        // 检查 0 - 3 字节是否为 "SEND"
        char first4[5] = {0};
        strncpy(first4, recv_buf, sizeof(first4));
        if (strcmp(first4, "SEND")) {
            printf("[error %s] first 4 != \"SEND\"\n", get_time());
            exit(-1);
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
        if (index >= recv_base && index < recv_next_seq) {
            recv_window[index] = 1;
        }
#endif

        // GBN: 检查接收到的分组是否是想要的
        if (excepted_index == index) {
            // 是想要的
            excepted_index++;
            printf("[info %s] Index %u accepted!\n", get_time(), index);
#ifdef TFILE
            // 写入文件，只兼容 GBN
            int recv_file_fd =
                open("recv.txt", O_RDWR | O_CREAT | O_APPEND, 0664);
            if (recv_file_fd == -1) {
                perror("[error] open");
                exit(-1);
            }
            char write_buf[1024] = {0};
            strcpy(write_buf, recv_buf + 8);
            strcat(write_buf, " ");
            write(recv_file_fd, write_buf, strlen(write_buf));
            close(recv_file_fd);
#endif
        } else {
#ifdef SR
            if (index >= recv_base && index < recv_next_seq) {
                // 不是想要的，但是缓存
                printf("[info %s] Index %u cached!\n", get_time(), index);
            } else {
                // 不是想要的，但在接收窗口之外，所以不缓存
                printf("[info %s] Index %u not cached!\n", get_time(), index);
            }
#else
            // 不是想要的，不发 ACK
            printf("[info %s] Index %u not accept, waiting for index %u!\n",
                   get_time(), index, excepted_index);
            continue;
#endif
        }

#ifdef SR
        // 移动接收窗口
        // 计算从 recv_base 开始有几个连续的缓存报文
        int cached_count = 0;
        for (unsigned i = recv_base; i < recv_next_seq; i++) {
            if (recv_window[i] == 1) {
                cached_count++;
            } else {
                break;
            }
        }
        recv_base += cached_count;
        recv_next_seq += cached_count;
        printf("[info %s] recv_base = %u, recv_next_seq = %u\n", get_time(),
               recv_base, recv_next_seq);
#endif

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
}
