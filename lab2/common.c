#include "common.h"

char* get_time() {
    size_t     len     = 20;
    char*      nowtime = (char*)malloc(len);
    time_t     rawtime;
    struct tm* ltime;

    time(&rawtime);
    ltime = localtime(&rawtime);
    strftime(nowtime, len, "%H:%M:%S", ltime);

    return nowtime;
}
