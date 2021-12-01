#include "sysInclude.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

extern void ip_DiscardPkt(char* pBuffer, int type);

extern void ip_SendtoLower(char* pBuffer, int length);

extern void ip_SendtoUp(char* pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students

int stud_ip_recv(char* pBuffer, unsigned short length) {
    // 版本号
    int version = pBuffer[0] >> 4;
    if (version != 4) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }

    // 头部长度
    int head_len = pBuffer[0] & 0xF;
    if (4 * head_len < 20) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }

    // TTL
    short ttl = (unsigned short)pBuffer[8];
    if (ttl == 0) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }

    // 目的地址
    int destination = ntohl(*(unsigned int*)(pBuffer + 16));
    if (destination != getIpv4Address() && destination != 0xFFFF) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }

    // 校验和
    short         checksum = ntohs(*(unsigned short*)(pBuffer + 10));
    unsigned long sum      = 0;
    unsigned long temp     = 0;
    int           i;
    for (i = 0; i < head_len * 2; i++) {
        temp += (unsigned char)pBuffer[i * 2] << 8;
        temp += (unsigned char)pBuffer[i * 2 + 1];
        sum += temp;
        temp = 0;
    }
    unsigned short l_word = sum & 0xffff;
    unsigned short h_word = sum >> 16;
    if (l_word + h_word != 0xFFFF) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }

    // 检查无误
    ip_SendtoUp(pBuffer, length);

    return 0;
}

int stud_ip_Upsend(char* pBuffer, unsigned short len, unsigned int srcAddr,
                   unsigned int dstAddr, byte protocol, byte ttl) {
    short ip_length = len + 20;

    // 发送分组
    char* buffer = (char*)malloc(ip_length);
    memset(buffer, 0, ip_length);

    buffer[0] = 0x45;
    buffer[8] = ttl;
    buffer[9] = protocol;

    unsigned short network_length = htons(ip_length);
    memcpy(buffer + 2, &network_length, 2);
    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);

    memcpy(buffer + 12, &src, 4);
    memcpy(buffer + 16, &dst, 4);

    unsigned long sum  = 0;
    unsigned long temp = 0;
    for (int i = 0; i < 20; i += 2) {
        temp += (unsigned char)buffer[i] << 8;
        temp += (unsigned char)buffer[i + 1];
        sum += temp;
        temp = 0;
    }
    unsigned short l_word          = sum & 0xffff;
    unsigned short h_word          = sum >> 16;
    unsigned short checksum        = l_word + h_word;
    checksum                       = ~checksum;
    unsigned short header_checksum = htons(checksum);
    memcpy(buffer + 10, &header_checksum, 2);
    memcpy(buffer + 20, pBuffer, len);
    ip_SendtoLower(buffer, len + 20);
    return 0;
}
