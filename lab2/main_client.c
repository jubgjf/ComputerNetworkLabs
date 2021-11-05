#include "client.h"
#include "common.h"
#include <pthread.h>
#include <stdio.h>

int main(int argc, char** argv) {
    // 给线程传参
    struct main_arg_port arg_port;
    struct main_arg      arg;
    arg.argc                    = argc;
    arg.argv                    = argv;
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

    pthread_exit(NULL);
}
