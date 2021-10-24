#include "proxy.h"

int main() {
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &signal_mask, NULL) == -1) {
        perror("SIG_PIPE");
    }

    // 监听客户的套接字
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("[error] socket");
        return -1;
    }

    // 防止出现 bind: Address already in use
    int on = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
        perror("[error] setsockopt");
    }

    // 绑定本地地址
    struct sockaddr_in proxy_saddr;
    memset(&proxy_saddr, 0, sizeof(proxy_saddr));
    proxy_saddr.sin_family      = AF_INET;
    proxy_saddr.sin_addr.s_addr = INADDR_ANY;
    proxy_saddr.sin_port        = htons(PROXY_PORT);
    if (bind(listen_fd, (struct sockaddr*)&proxy_saddr, sizeof(proxy_saddr)) ==
        -1) {
        perror("[error] bind");
        return -1;
    }

    // 监听客户
    if (listen(listen_fd, 8) == -1) {
        perror("[error] listen");
        exit(-1);
    }

    // 创建缓存目录
    if (create_cache_dir() == -1) {
        perror("[error] mkdir");
        return -1;
    }

    // 为连接代理服务器的客户创建线程
    while (1) {
        printf("[wait] Waiting for connection\n");
        struct sockaddr_in clientaddr;
        int                clientaddr_len = sizeof(clientaddr);

        int cfd = accept(listen_fd, (struct sockaddr*)(&clientaddr),
                         (socklen_t*)(&clientaddr_len));
        if (cfd == -1) {
            perror("[error] accept");
            continue; // 不影响其他客户端的连接
        }

        printf("[new] A new connection occurs!\n");
        printf("[info] Client ip = %s, port = %hu\n",
               inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

        // 客户地址和套接字
        struct fd_addr fa;
        fa.addr = clientaddr;
        fa.fd   = cfd;

        // 创建线程
        pthread_t tid;
        if (pthread_create(&tid, NULL, (void*)(&threaded_proxy),
                           (void*)(&fa)) == -1) {
            perror("[error] pthread");
            break;
        }
    }

    // 关闭文件描述符
    shutdown(listen_fd, SHUT_RDWR);

    return 0;
}

int create_cache_dir() {
    if (!opendir(CACHE_DIR)) //判断目录是否存在
    {
        if (mkdir(CACHE_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            return -1;
        } else {
            printf("[info] Cache dir created!\n");
            return 0;
        }
    } else {
        printf("[info] Cache dir exist!\n");
        return 1;
    }
}

void threaded_proxy(void* fd_addr) {
    // 客户套接字和地址
    struct fd_addr     fa         = *((struct fd_addr*)fd_addr);
    int                cfd        = fa.fd;
    struct sockaddr_in clientaddr = fa.addr;

    while (1) {
        printf("[info] Waiting for client message\n");

        // 读取客户端请求
        char recv_buf[1024] = {0};
        int  len            = read(cfd, recv_buf, 1024);
        if (len == 0) {
            printf("[warn] Client may closed\n");
            break;
        } else if (len == -1) {
            perror("[error] read");
            break;
        }
        if (recv_buf[0] == 'C' && recv_buf[1] == 'O') {
            // https 请求以 "CONNECT" 开头，不支持 https 请求
            printf("[https] https not support!\n");
            break;
        }
        printf("[info] Read from client:\n");
        printf("========================================\n");
        printf("%s", recv_buf);
        printf("========================================\n");

        // 用户过滤
        if (fliter_user(clientaddr, cfd)) {
            break;
        }

        // 网站过滤
        if (fliter_host(recv_buf, cfd)) {
            break;
        }

        // 钓鱼网站
        fake_host(recv_buf);

        // 与远程建立连接
        int server_fd = fetch(recv_buf);
        if (server_fd == -1) {
            printf("[error] server_fd error\n");
            break;
        }
        write(server_fd, recv_buf, strlen(recv_buf));

        // 获取缓存信息
        int  cache_valid         = 0;   // 缓存是否有效
        int  cache_fd            = -1;  // 缓存文件描述符
        char cache_filepath[100] = {0}; // 缓存文件路径
        if (cache(&cache_valid, cache_filepath, &cache_fd, recv_buf) == -1) {
            break;
        }

        char buffer[1024] = {0};
        if (cache_valid) {
            // 转发缓存给客户
            printf("[cache] Use cache\n");
            while ((len = read(cache_fd, buffer, sizeof(buffer))) > 0) {
                write(cfd, buffer, len);
            }
        } else {
            // 转发远程响应给客户
            printf("[cache] Use remote\n");
            while ((len = read(server_fd, buffer, sizeof(buffer))) > 0) {
                write(cfd, buffer, len);
                if (cache_fd != -1) {
                    write(cache_fd, buffer, len);
                }
            }
        }

        // 关闭远程套接字
        shutdown(server_fd, SHUT_RDWR);
        // 关闭缓存描述符
        close(cache_fd);
    }

    // 关闭客户套接字
    shutdown(cfd, SHUT_RDWR);

    printf("[warn] End of thread\n");
}

void parse_host(char* head, char* hostname, char* url) {
    // 请求头第一行：请求路径所在行
    // 示例：GET http://www.baidu.com/ HTTP/1.1
    char* head_line1 = strtok_r(head, "\n", &head);
    char* raw_url    = strtok_r(head_line1, " ", &head_line1);
    raw_url          = strtok_r(head_line1, " ", &head_line1);
    // 请求头第二行：主机名所在行
    // 示例：Host: www.baidu.com
    char* head_line2 = strtok_r(head, "\n", &head);

    strcpy(hostname, head_line2 + 6); // 删除 "Host: "
    for (int i = 0;; i++) {
        if (hostname[i] == '\r') {
            hostname[i] = '\0';
            break;
        }
    }

    strcpy(url, raw_url + 7); // 删除 "http://"
}

int fetch(char* head) {
    // 分析客户端想要访问的主机信息
    char head_parse[1024] = {0};
    strcpy(head_parse, head);
    char hostname[100] = {0}; // 主机名
    char url[100]      = {0}; // 详细 URL
    parse_host(head_parse, hostname, url);

    printf("[info] Request hostname = %s\n", hostname);
    printf("[info] Request url      = %s\n", url);

    // 获取客户端想要访问的主机信息结构体
    struct hostent* host = gethostbyname(hostname);
    if (!host) {
        printf("[error] gethostbyname fail\n");
        return -1;
    }

    // 创建面向远程的 socket
    int pfd = socket(host->h_addrtype, SOCK_STREAM, 0);
    if (pfd == -1) {
        perror("[error] socket");
        return -1;
    }

    //向远程服务器发起请求
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
    server_addr.sin_port = htons(80);
    if (connect(pfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) ==
        -1) {
        perror("[error] connect");
        return -1;
    }

    return pfd;
}

void strrpl(char* source, char dest, char rpl) {
    int i = 0;
    while (source[i] != '\0') {
        if (source[i] == dest) {
            source[i] = rpl;
        }
        i++;
    }
}

void gen_cache_filepath(char* head, char* cache_filepath) {
    // 分析客户端想要访问的主机信息
    char head_parse[1024] = {0};
    strcpy(head_parse, head);
    char hostname[100] = {0}; // 主机名
    char url[100]      = {0}; // 详细 URL
    parse_host(head_parse, hostname, url);

    char cache_filename[100] = {0};
    strcpy(cache_filename, url);
    strrpl(cache_filename, '/', '-');
    strcat(cache_filepath, CACHE_DIR);
    strcat(cache_filepath, cache_filename);
}

void strpop(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    str[i - 1] = '\0';
}

int fliter_user(struct sockaddr_in addr_in, int cfd) {
#ifdef FLITER_USER
    int         block_users_count = 1;
    const char* block_users[]     = {"127.0.0.1"};

    for (int i = 0; i < block_users_count; i++) {
        if (!strcmp(inet_ntoa(addr_in.sin_addr), block_users[i])) {
            printf("[fliter] User is blocked!\n");
            char msg[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                         "\r\nYou are not allowed to access this!\r\n\r\n";
            write(cfd, msg, strlen(msg));
            return 1;
        }
    }
#endif

    return 0;
}

int fliter_host(char* head, int cfd) {
#ifdef FLITER_HOST
    // 分析客户端想要访问的主机信息
    char head_parse[1024] = {0};
    strcpy(head_parse, head);
    char hostname[100] = {0}; // 主机名
    char url[100]      = {0}; // 详细 URL
    parse_host(head_parse, hostname, url);

    int         block_hosts_count = 2;
    const char* block_hosts[]     = {"www.baidu.com", "cs.hit.edu.cn"};
    for (int i = 0; i < block_hosts_count; i++) {
        if (!strcmp(hostname, block_hosts[i])) {
            printf("[fliter] Host is blocked!\n");
            char msg[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                         "\r\nThis host is not allowed to access!\r\n\r\n";
            write(cfd, msg, strlen(msg));
            return 1;
        }
    }
#endif

    return 0;
}

void fake_host(char* head) {
#ifdef FAKE_HOST
    // 分析客户端想要访问的主机信息
    char head_parse[1024] = {0};
    strcpy(head_parse, head);
    char hostname[100] = {0}; // 主机名
    char url[100]      = {0}; // 详细 URL
    parse_host(head_parse, hostname, url);

    int         block_hosts_count = 1;
    const char* block_hosts[]     = {"www.gov.cn"};
    const char* fake_host         = "www.baidu.com";
    for (int i = 0; i < block_hosts_count; i++) {
        if (!strcmp(hostname, block_hosts[i])) {
            printf("[fliter] Client redirect!\n");
            memset(hostname, 0, strlen(hostname));
            strcpy(hostname, fake_host);
            printf("[fliter] Fake new hostname = %s\n", hostname);
            memset(head, 0, strlen(head));
            strcpy(head, "GET http://www.baidu.com/ HTTP/1.1\r\n"
                         "Host: www.baidu.com\r\n"
                         "User-Agent: curl/7.79.1\r\n"
                         "Accept: */*\r\n"
                         "Proxy-Connection: Keep-Alive\r\n\r\n");
            break;
        }
    }
#endif
}

int cache(int* cache_valid, char* cache_filepath, int* cache_fd, char* head) {
    // 缓存文件路径
    gen_cache_filepath(head, cache_filepath);

    // 一些二进制文件不作为缓存储存
    if (strstr(cache_filepath, "jpg") || strstr(cache_filepath, "png") ||
        strstr(cache_filepath, "gif") || strstr(cache_filepath, "ico")) {
        printf("[warn] Cache format not support!\n");
        *cache_valid = 0;
        *cache_fd    = -1;
        return 0;
    }

    // 缓存是否有效
    *cache_valid = 0;

    // 文件信息结构体
    struct stat statbuf;
    if (!stat(cache_filepath, &statbuf)) {
        // 存在缓存
        printf("[info] Cache exist!\n");

        // 解析缓存中的 Last-Modified 字段
        char cache_content[1024] = {0}; // 缓存内容
        char lmdf_field[100]     = {0}; // Last-Modified 字段
        int  cache_fd_lmdf       = open(cache_filepath, O_RDWR | O_CREAT, 0664);
        if (cache_fd_lmdf == -1) {
            perror("[error] open");
            return -1;
        }
        if (read(cache_fd_lmdf, cache_content, sizeof(cache_content)) > 0) {
            char* raw_field;
            if ((raw_field = strstr(cache_content, "Last-Modified: "))) {
                strncpy(lmdf_field, raw_field + strlen("Last_Modified: "),
                        sizeof(lmdf_field));
                for (int i = 0;; i++) {
                    if (lmdf_field[i] == '\r') {
                        lmdf_field[i] = '\0';
                        break;
                    }
                }
                printf("[info] Cache Last-Modified: %s\n", lmdf_field);
                *cache_valid = 1;
            }
        }

        // 构造条件 get 判断缓存是否有效
        char cget_head[1024] = {0};
        strcpy(cget_head, head);
        strpop(cget_head); // 删除 '\n'
        strpop(cget_head); // 删除 '\r'
        strcat(cget_head, "If-Modified-Since: ");
        strcat(cget_head, lmdf_field);
        strcat(cget_head, "\r\n\r\n");

        // 发送条件 get
        int server_cget_fd = fetch(head);
        if (server_cget_fd == -1) {
            printf("[error] server_cget_fd error\n");
            return -1;
        }
        write(server_cget_fd, cget_head, strlen(cget_head));
        char cget_response[1024] = {0};
        printf("[info] Conditional get start\n");
        if (read(server_cget_fd, cget_response, sizeof(cget_response)) > 0) {
            if (strstr(cget_response, "HTTP/1.1 304 Not Modified")) {
                printf("[info] Cache valid!\n");
                *cache_valid = 1;
            }
        }
        printf("[info] Conditional get end\n");
        if (*cache_valid == 0) {
            printf("[info] Cache invalid!\n");
        }
        shutdown(server_cget_fd, SHUT_RDWR);
    } else {
        // 不存在缓存
        printf("[info] Cache not exist!\n");
        *cache_valid = 0;
    }

    // 缓存文件描述符
    *cache_fd = open(cache_filepath, O_RDWR | O_CREAT, 0664);
    if (*cache_fd == -1) {
        perror("[error] open");
        return -1;
    }

    return 0;
}
