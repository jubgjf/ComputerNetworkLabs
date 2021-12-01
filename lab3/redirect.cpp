#include "sysInclude.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using std::vector;

// system support
extern void fwd_LocalRcv(char* pBuffer, int length);

extern void fwd_SendtoLower(char* pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char* pBuffer, int type);

extern unsigned int getIpv4Address();

// implemented by students

vector<stud_route_msg> route;

void stud_Route_Init() { return; }

void stud_route_add(stud_route_msg* proute) {
    stud_route_msg temp;
    unsigned       dest    = ntohl(proute->dest);
    unsigned       masklen = ntohl(proute->masklen);
    unsigned       nexthop = ntohl(proute->nexthop);
    temp.dest              = dest;
    temp.masklen           = masklen;
    temp.nexthop           = nexthop;
    route.push_back(temp);
}

int stud_fwd_deal(char* pBuffer, int length) {
    // 版本号
    int version = pBuffer[0] >> 4;

    // 首部长度
    int head_length = pBuffer[0] & 0xF;

    // TTL
    short ttl = (unsigned short)pBuffer[8];
    if (ttl <= 0) {
        // 保证分组能够转发出去
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }

    // 校验和
    short checksum = ntohs(*(unsigned short*)(pBuffer + 10));

    // 目的地址
    int dest_ip = ntohl(*(unsigned*)(pBuffer + 16));
    if (dest_ip == getIpv4Address()) {
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }

    stud_route_msg* ans_route = NULL;
    int             temp_dest = dest_ip;
    for (int i = 0; i < route.size(); i++) {
        unsigned temp_sub_net =
            route[i].dest & ((1 << 31) >> (route[i].masklen - 1));

        if (temp_sub_net == temp_dest) {
            ans_route = &route[i];
            break;
        }
    }

    if (!ans_route) {
        // 找不到转发目标
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);

        return 1;
    } else {
        // 发送的分组
        char* buffer = (char*)malloc(length);
        memcpy(buffer, pBuffer, length);

        // TTL
        buffer[8] = ttl - 1;
        memset(buffer + 10, 0, 2);

        // 校验和
        unsigned long sum  = 0;
        unsigned long temp = 0;
        for (int i = 0; i < head_length * 2; i++) {
            temp += (unsigned char)buffer[i * 2] << 8;
            temp += (unsigned char)buffer[i * 2 + 1];
            sum += temp;
            temp = 0;
        }
        unsigned short l_word          = sum & 0xFFFF;
        unsigned short h_word          = sum >> 16;
        unsigned short checksum        = l_word + h_word;
        checksum                       = ~checksum;
        unsigned short header_checksum = htons(checksum);
        memcpy(buffer + 10, &header_checksum, 2);

        fwd_SendtoLower(buffer, length, ans_route->nexthop);

        return 0;
    }
}
