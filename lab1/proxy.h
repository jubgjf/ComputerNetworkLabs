#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 代理服务器监听端口
#define PROXY_PORT 10000

// 缓存目录
#define CACHE_DIR "cache/"

/**
 * @brief 套接字通信时的描述符和对方地址
 *
 */
struct fd_addr {
    int                fd;
    struct sockaddr_in addr;
};

/**
 * @brief 创建缓存目录
 *
 * @return int 若创建失败，则返回 -1；
 *             若目录已存在，则返回 1；
 *             若创建成功，则返回 0；
 */
int create_cache_dir();

/**
 * @brief 代理服务器为每一个连接的客户启动的线程
 *
 * @param fd_addr 客户端文件描述符和客户地址结构体
 */
void threaded_proxy(void* fd_addr);

/**
 * @brief 从请求头中获取主机信息
 *
 * @param head 请求头
 * @param hostname 获取到的主机名将被存放于此
 * @param url 获取到的 URL 将被存放于此
 */
void parse_host(char* head, char* hostname, char* url);

/**
 * @brief 获取面向远程的套接字描述符
 *
 * @param head 请求头
 * @return int 若无错误则返回面向远程的套接字描述符；否则返回 -1
 */
int fetch(char* head);

/**
 * @brief 字符串替换
 *
 * @param source 需要处理的字符串
 * @param dest 需要被替换的字符
 * @param rpl 替换用到的字符
 */
void strrpl(char* source, char dest, char rpl);

/**
 * @brief 生成请求 URL 对应的缓存路径
 *
 * @param head 请求头
 * @param cache_filepath 缓存路径存放于此
 */
void gen_cache_filepath(char* head, char* cache_filepath);

/**
 * @brief 删除字符串最后一个字符
 *
 * @param str 字符串
 */
void strpop(char* str);

/**
 * @brief 用户过滤：不支持某些用户访问外部网站
 *
 * @param addr_in 客户地址结构体
 * @param cfd 客户套接字
 * @return int 若返回1，不允许访问；否则可以访问
 */
int fliter_user(struct sockaddr_in addr_in, int cfd);

/**
 * @brief 网站过滤：不允许访问某些网站
 *
 * @param head 请求头
 * @param cfd 客户套接字
 * @return int 若返回1，不允许访问；否则可以访问
 */
int fliter_host(char* head, int cfd);

/**
 * @brief 钓鱼网站
 *
 * @param head 请求头
 */
void fake_host(char* head);

/**
 * @brief 获取缓存信息
 *
 * @param cache_valid 缓存是否有效将存放于此
 * @param cache_filepath 缓存文件路径将存放于此
 * @param cache_fd 缓存文件描述符将存放于此
 * @param head 请求头
 * @return int 若有错误则返回 -1；否则返回 0
 */
int cache(int* cache_valid, char* cache_filepath, int* cache_fd, char* head);
