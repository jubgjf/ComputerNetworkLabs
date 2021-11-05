#include "client.h"
#include "common.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief 发送方窗口
 */
int sender_window[1024] = {0};

/**
 * @brief 发送方窗口的读写锁
 */
pthread_rwlock_t sender_window_lock;

int main(int argc, char** argv) {
#ifdef SR
    printf("[info %s] Running in SR!\n", get_time());
#else
    printf("[info %s] Running in GBN!\n", get_time());
#endif

    // 发送窗口起始序号
    unsigned base = 0;

    // 发送窗口下一个序号
    unsigned next_seq = base + SENDER_WINDOW_SIZE;

    // 初始化读写锁
    pthread_rwlock_init(&sender_window_lock, NULL);

    // 发送窗口各个线程对应的线程号
    pthread_t window_tid[argc - 1];
    memset(window_tid, 0, sizeof(window_tid));

    while (base <= (unsigned)argc - 2) {
        // 为发送窗口的每一个报文建立一个线程
        pthread_rwlock_wrlock(&sender_window_lock);
        for (unsigned p = base; p < next_seq; p++) {
            if (window_tid[p] != 0) {
                // 该发送线程已经被创建过
                continue;
            }

            if ((int)p >= argc - 1) {
                // 没有报文可传输，窗口右侧超出边界
                break;
            }

            // 报文信息
            struct message_seg s;
            strcpy(s.msg, argv[p + 1]);
            s.index = p;

            // 创建连接线程
            printf("[info %s] Create thread for index %u!\n", get_time(), p);
            if (pthread_create(&window_tid[p], NULL, (void*)(&threaded_client),
                               (void*)(&s)) == -1) {
                perror("[error] pthread");
                break;
            }

            // 增加线程间的间隔，否则日志格式混乱，不便展示
            usleep(500);
        }
        pthread_rwlock_unlock(&sender_window_lock);

        // 等待最前端的发送线程结束
        // 由于线程只能通过收到 ACK 结束，所以此时窗口一定需要更新
        printf("[info %s] Start to wait thread index %u!\n", get_time(), base);
        pthread_join(window_tid[base], NULL);

        // 检查最后一个 ACK 的报文序号
        pthread_rwlock_rdlock(&sender_window_lock);
        unsigned last_ack_index = 0xFFFFFFFF;
        for (int i = 0; i < argc - 1; i++) {
            if (sender_window[i] == 1) {
                last_ack_index = i;
            }
        }
        pthread_rwlock_unlock(&sender_window_lock);

        // 更新窗口
        if (last_ack_index != 0xFFFFFFFF) {
            base     = last_ack_index + 1;
            next_seq = base + SENDER_WINDOW_SIZE;
        } else {
            printf("[info %s] Window not update!\n", get_time());
        }

        // 当前窗口进度
        pthread_rwlock_rdlock(&sender_window_lock);
        printf("[info %s] Progress: [", get_time());
        for (int i = 0; i < argc - 1; i++) {
            if (sender_window[i] == 1) {
                printf("#");
            } else {
                printf(".");
            }
        }
        printf("]\n");
        printf("[info %s] base = %u, next_seq = %u\n", get_time(), base,
               next_seq);
        pthread_rwlock_unlock(&sender_window_lock);
    }

    pthread_rwlock_destroy(&sender_window_lock);
    pthread_exit(NULL);
}

void threaded_client(void* seg) {
    // 待发送的消息信息
    struct message_seg s       = *((struct message_seg*)seg);
    char*              content = s.msg;
    unsigned           index   = s.index;

    // 发起连接的套接字
    int request_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (request_fd == -1) {
        perror("[error] socket");
        return;
    }

    // 防止出现 bind: Address already in use
    int on = 1;
    if (setsockopt(request_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) <
        0) {
        perror("[error] setsockopt reuseaddr");
        return;
    }

    // 设置 recvfrom 在超时 1s 后直接返回
    struct timeval read_timeout;
    read_timeout.tv_sec  = 1;
    read_timeout.tv_usec = 0;
    if (setsockopt(request_fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout,
                   sizeof read_timeout) == -1) {
        perror("[error] setsockopt timeout");
        return;
    }

    // 对服务器发起连接
    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port        = htons(SERVER_PORT);

    // 超时次数
    int timeout_count = 0;

    while (1) {
        if (timeout_count >= 20) {
            printf("[error %s] Too many timeout!\n", get_time());
            exit(-1);
        }

        // 发送消息
        char send_buf[1024] = {0};

        // 0 - 3 字节为 "SEND"
        strcat(send_buf, "SEND");

        // 4 - 7 字节为序号
        send_buf[4] = (index >> 24) & 0xFF;
        send_buf[5] = (index >> 16) & 0xFF;
        send_buf[6] = (index >> 8) & 0xFF;
        send_buf[7] = (index >> 0) & 0xFF;

        // 正文内容
        strcpy(send_buf + 8, content);

        // 发送报文
        sendto(request_fd, send_buf, sizeof(send_buf), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));
        printf("%27s+--------------------------------\n", " ");
        printf("[info %s] Server <== | SEND %u %s\n", get_time(), index,
               content);
        printf("%27s+--------------------------------\n", " ");

        // 接收回应
        char recv_buf[1024];
        if (recvfrom(request_fd, recv_buf, sizeof(recv_buf), 0, NULL, 0) ==
            -1) {
            printf("[warn %s] Server may timeout!\n", get_time());
            printf("[info %s] Attempt to resend!\n", get_time());
            // 超时，则发送方重传
            timeout_count++;
            continue;
        }

        // 检查 0 - 3 字节为否为 "ACKK"
        char first4[5] = {0};
        strncpy(first4, recv_buf, sizeof(first4));
        if (strcmp(first4, "ACKK")) {
            printf("[error %s] first 4 != \"ACKK\"\n", get_time());
            return;
        }

        // 检查 4 - 7 字节的序号
        unsigned recv_index = 0;
        recv_index |= recv_buf[4] << 24;
        recv_index |= recv_buf[5] << 16;
        recv_index |= recv_buf[6] << 8;
        recv_index |= recv_buf[7] << 0;
        if (recv_index != index) {
            printf("[warn %s] Recv_index(%u) != index\n", get_time(),
                   recv_index);
        }

        pthread_rwlock_wrlock(&sender_window_lock);
#ifdef SR
        sender_window[recv_index] = 1;
#else
        // 收到 recv_index，则 recv_index 之前的报文都已经被收到
        for (unsigned i = 0; i < recv_index + 1; i++) {
            sender_window[i] = 1;
        }
#endif
        pthread_rwlock_unlock(&sender_window_lock);

        // 8 字节之后的正文
        char content[50] = {0};
        strcpy(content, recv_buf + 8);
        printf("%27s+--------------------------------\n", " ");
        printf("[info %s] Server ==> | ACKK %u %s\n", get_time(), recv_index,
               content);
        printf("%27s+--------------------------------\n", " ");

        break;
    }

    close(request_fd);
}
