#include "common.h"
#include "server.h"
#include <pthread.h>
#include <stdio.h>

int main() {
    // 给线程传参
    struct main_arg_port arg_port;
    struct main_arg      arg;
    arg.argc                    = 0;
    arg.argv                    = NULL;
    arg_port.arg                = arg;
    arg_port.local_server_port  = 10000;
    arg_port.remote_server_port = 0;

    // 创建接收方线程
    pthread_t tid;
    if (pthread_create(&tid, NULL, (void*)(&server), (void*)(&arg_port)) ==
        -1) {
        perror("[error] pthread");
        return -1;
    }

    pthread_exit(NULL);
}
