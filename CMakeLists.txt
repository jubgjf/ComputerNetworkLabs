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

# <pthread.h> 多线程
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(lab1 PRIVATE Threads::Threads)
