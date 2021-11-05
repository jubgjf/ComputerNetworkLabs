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

void strpop(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    str[i - 1] = '\0';
}
