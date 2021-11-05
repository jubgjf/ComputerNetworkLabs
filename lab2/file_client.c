#include "client.h"
#include "common.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    // 打开文件
    char send_file_buf[10240] = {0};
    int  send_file_fd         = open("../lab2/send.txt", O_RDWR, 0664);
    if (send_file_fd == -1) {
        perror("[error] open");
        return -1;
    }

    // 读取内容
    int len = 0;
    if ((len = read(send_file_fd, send_file_buf, sizeof(send_file_buf))) > 0) {
        // 实际报文
        char* messages[1024] = {0};
        char* p              = NULL;
        int   count          = 1;
        memset(messages, 0, sizeof(messages));
        messages[count] = strtok(send_file_buf, " ");
        for (count = 2; (p = strtok(NULL, " ")); count++) {
            messages[count] = p;
        }

        // 给线程传参
        struct main_arg_port arg_port;
        struct main_arg      arg;
        arg.argc                    = count;
        arg.argv                    = messages;
        arg_port.arg                = arg;
        arg_port.local_server_port  = 0;
        arg_port.remote_server_port = 10000;

        // 创建发送方线程
        pthread_t tid;
        if (pthread_create(&tid, NULL, (void*)(&client), (void*)(&arg_port)) ==
            -1) {
            perror("[error] pthread");
            return -1;
        }
    }

    pthread_exit(NULL);
}
