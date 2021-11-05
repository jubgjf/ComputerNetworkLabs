cmake_minimum_required(VERSION 3.0.0)
project(ComputerNetworkLabs VERSION 0.1.0)

add_compile_options(-Wall -Wextra)

# ===== Lab 1 =====

add_executable(lab1 lab1/proxy.c lab1/proxy.h)

# 用户过滤
option(FLITER_USER "fliter user" OFF)
if(FLITER_USER)
    add_definitions(-DFLITER_USER)
endif(FLITER_USER)

# 网站过滤
option(FLITER_HOST "fliter host" OFF)
if(FLITER_HOST)
    add_definitions(-DFLITER_HOST)
endif(FLITER_HOST)

# 钓鱼网站
option(FAKE_HOST "fake host" OFF)
if(FAKE_HOST)
    add_definitions(-DFAKE_HOST)
endif(FAKE_HOST)

target_link_libraries(lab1 PRIVATE Threads::Threads)


# ===== Lab 2 =====

# 丢包
option(LOSS "loss" OFF)
if(LOSS)
    add_definitions(-DLOSS)
endif(LOSS)

# SR 协议
option(SR "sr" OFF)
if(SR)
    add_definitions(-DSR)
endif(SR)

# 文件传输
option(TFILE "file" OFF)
if(TFILE)
    add_definitions(-DTFILE)
endif(TFILE)

# C/S 结构
add_executable(
    lab2_server
    lab2/main_server.c
    lab2/common.c
    lab2/common.h
    lab2/server.c
    lab2/server.h
)
target_link_libraries(lab2_server PRIVATE Threads::Threads)

add_executable(
    lab2_client
    lab2/main_client.c
    lab2/common.c
    lab2/common.h
    lab2/client.c
    lab2/client.h
)
target_link_libraries(lab2_client PRIVATE Threads::Threads)

# 双向传输
add_executable(
    lab2_peer
    lab2/peer.c
    lab2/common.c
    lab2/common.h
    lab2/client.c
    lab2/client.h
    lab2/server.c
    lab2/server.h
)
target_link_libraries(lab2_peer PRIVATE Threads::Threads)

# 文件传输
add_executable(
    lab2_file_client
    lab2/file_client.c
    lab2/common.c
    lab2/common.h
    lab2/client.c
    lab2/client.h
)
target_link_libraries(lab2_file_client PRIVATE Threads::Threads)

# ===== <pthread.h> 多线程 =====
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
