#ifndef TUN_LAB_COMMON_H
#define TUN_LAB_COMMON_H

#define REMOTE_ADDR "172.26.50.199"
#define REMOTE_PORT 8080

#define LOCAL_ADDR "10.0.0.2"
#define LOCAL_PORT 16888

#define IP_HDR_LEN 20
#define TCP_HDR_LEN 20

#define START_SEQ 114514

#pragma pack(push, 1)
struct _IPv4HDR {
    BYTE Ipv4Preamble;
    BYTE Unused1;
    USHORT TotalLength;
    ULONG Unused2;
    BYTE TTL;
    BYTE Protocol;
    USHORT Checksum;
    ULONG SrcAddr;
    ULONG DstAddr;
};
#pragma pack(pop)
typedef struct _IPv4HDR IPv4HDR;


#pragma pack(push, 1)
struct _TCPHDR {
    USHORT SrcPort;
    USHORT DstPort;
    ULONG Seq;
    ULONG Ack;
    USHORT DO_Reserved_Flags;
    USHORT Window;
    USHORT Checksum;
    USHORT Urgent;
};
#pragma pack(pop)
typedef struct _TCPHDR TCPHDR;

#endif //TUN_LAB_COMMON_H
