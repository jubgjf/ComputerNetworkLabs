#include "client.h"
#include "common.h"
#include "server.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("[info %s] Argc != 3!\n", get_time());
        return -1;
    }

    // 本地接收方端口号
    uint16_t local_server_port = atoi(argv[1]);

    // 远程接收方端口号
    uint16_t remote_server_port = atoi(argv[2]);

    while (1) {
        // 读取用户输入
        printf("[info %s] Please input >>", get_time());
        char stdin_buf[1024] = {0};
        fgets(stdin_buf, sizeof(stdin_buf), stdin);
        strpop(stdin_buf); // 去除换行符

        // 实际报文
        char* messages[1024] = {0};
        char* p              = NULL;
        int   count          = 1;
        memset(messages, 0, sizeof(messages));
        messages[count] = strtok(stdin_buf, " ");
        for (count = 2; (p = strtok(NULL, " ")); count++) {
            messages[count] = p;
        }

        // 给线程传参
        struct main_arg_port arg_port;
        struct main_arg      arg;
        arg.argc                    = count;
        arg.argv                    = messages;
        arg_port.arg                = arg;
        arg_port.local_server_port  = local_server_port;
        arg_port.remote_server_port = remote_server_port;

        // 创建发送方线程
        pthread_t send_tid;
        if (pthread_create(&send_tid, NULL, (void*)(&client),
                           (void*)(&arg_port)) == -1) {
            perror("[error] pthread");
            return -1;
        }

        // 创建接收方线程
        pthread_t recv_tid;
        if (pthread_create(&recv_tid, NULL, (void*)(&server),
                           (void*)(&arg_port)) == -1) {
            perror("[error] pthread");
            return -1;
        }
    }

    pthread_exit(NULL);
}
