#include <stdio.h>
#include "wintun.h"
#include "common.h"
#include <winsock2.h>
#include <winternl.h>
#include <netioapi.h>

static WINTUN_CREATE_ADAPTER_FUNC *WintunCreateAdapter;
static WINTUN_CLOSE_ADAPTER_FUNC *WintunCloseAdapter;
static WINTUN_OPEN_ADAPTER_FUNC *WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC *WintunGetAdapterLUID;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC *WintunGetRunningDriverVersion;
static WINTUN_DELETE_DRIVER_FUNC *WintunDeleteDriver;
static WINTUN_SET_LOGGER_FUNC *WintunSetLogger;
static WINTUN_START_SESSION_FUNC *WintunStartSession;
static WINTUN_END_SESSION_FUNC *WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC *WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC *WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC *WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC *WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC *WintunSendPacket;

static unsigned long local_addr = 0;

static HMODULE
InitializeWintun(void)
{
    HMODULE Wintun =
            LoadLibraryExW(L"..\\lib\\wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!Wintun)
        return NULL;
#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
    if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
        X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
        X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
        X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
    {
        DWORD LastError = GetLastError();
        FreeLibrary(Wintun);
        SetLastError(LastError);
        return NULL;
    }
    return Wintun;
}

static void CALLBACK
ConsoleLogger(_In_ WINTUN_LOGGER_LEVEL Level, _In_ DWORD64 Timestamp, _In_z_ const WCHAR *LogLine)
{
    SYSTEMTIME SystemTime;
    FileTimeToSystemTime((FILETIME *)&Timestamp, &SystemTime);
    WCHAR LevelMarker;
    switch (Level)
    {
        case WINTUN_LOG_INFO:
            LevelMarker = L'+';
            break;
        case WINTUN_LOG_WARN:
            LevelMarker = L'-';
            break;
        case WINTUN_LOG_ERR:
            LevelMarker = L'!';
            break;
        default:
            return;
    }
    fwprintf(
            stderr,
            // Must set the format as %ls to work on MinGW64
            L"%04u-%02u-%02u %02u:%02u:%02u.%04u [%c] %ls\n",
            SystemTime.wYear,
            SystemTime.wMonth,
            SystemTime.wDay,
            SystemTime.wHour,
            SystemTime.wMinute,
            SystemTime.wSecond,
            SystemTime.wMilliseconds,
            LevelMarker,
            LogLine);
}

static DWORD64 Now(VOID)
{
    LARGE_INTEGER Timestamp;
    NtQuerySystemTime(&Timestamp);
    return Timestamp.QuadPart;
}

static DWORD
LogError(_In_z_ const WCHAR *Prefix, _In_ DWORD Error)
{
    WCHAR *SystemMessage = NULL, *FormattedMessage = NULL;
    FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            NULL,
            HRESULT_FROM_SETUPAPI(Error),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (void *)&SystemMessage,
            0,
            NULL);
    FormatMessageW(
            FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
            FORMAT_MESSAGE_MAX_WIDTH_MASK,
            SystemMessage ? L"%1: %3(Code 0x%2!08X!)" : L"%1: Code 0x%2!08X!",
            0,
            0,
            (void *)&FormattedMessage,
            0,
            (va_list *)(DWORD_PTR[]){ (DWORD_PTR)Prefix, (DWORD_PTR)Error, (DWORD_PTR)SystemMessage });
    if (FormattedMessage)
        ConsoleLogger(WINTUN_LOG_ERR, Now(), FormattedMessage);
    LocalFree(FormattedMessage);
    LocalFree(SystemMessage);
    return Error;
}

static DWORD
LogLastError(_In_z_ const WCHAR *Prefix)
{
    DWORD LastError = GetLastError();
    LogError(Prefix, LastError);
    SetLastError(LastError);
    return LastError;
}

static void
Log(_In_ WINTUN_LOGGER_LEVEL Level, _In_z_ const WCHAR *Format, ...)
{
    WCHAR LogLine[0x200];
    va_list args;
    va_start(args, Format);
    _vsnwprintf_s(LogLine, _countof(LogLine), _TRUNCATE, Format, args);
    va_end(args);
    ConsoleLogger(Level, Now(), LogLine);
}

static USHORT
IPv4Checksum(_In_reads_bytes_(Len) const IPv4HDR *Header, _In_ DWORD Len)
{
    ULONG Sum = 0;
    const BYTE *Buffer = (const BYTE *) Header;
    for (; Len > 1; Len -= 2, Buffer += 2)
        Sum += *(USHORT *)Buffer;
    if (Len)
        Sum += *Buffer;
    Sum = (Sum >> 16) + (Sum & 0xffff);
    Sum += (Sum >> 16);
    return (USHORT)(~Sum);
}

static USHORT
TcpChecksum(_In_reads_bytes_(Len) const BYTE *IpBuffer, _In_ DWORD Len)
{
    DWORD SegLen = Len - IP_HDR_LEN;
    ULONG Sum = 0;
    // Src addr
    Sum += *(USHORT *)(IpBuffer + 12);
    Sum += *(USHORT *)(IpBuffer + 14);
    // Dst addr
    Sum += *(USHORT *)(IpBuffer + 16);
    Sum += *(USHORT *)(IpBuffer + 18);
    // Reserved + Protocol
    Sum += htons((USHORT) IpBuffer[9]);
    // TCP seg length
    Sum += htons(SegLen);
    // TCP Seg
    const BYTE *Buffer = IpBuffer + IP_HDR_LEN;
    for (; SegLen > 1; SegLen -= 2, Buffer += 2)
        Sum += *(USHORT *)Buffer;
    if (SegLen)
        Sum += *Buffer;
    // Wrap around
    Sum = (Sum >> 16) + (Sum & 0xffff);
    Sum += (Sum >> 16);
    return (USHORT)(~Sum);
}

static void
SetTcpHeader(TCPHDR *Header, USHORT SrcPort, USHORT DstPort)
{
    memset(Header, 0, TCP_HDR_LEN);
    Header->SrcPort = SrcPort;
    Header->DstPort = DstPort;
    Header->DO_Reserved_Flags = htons(TCP_HDR_LEN >> 2 << 12);
    Header->Window = htons(65530);
}

static void
SetIpHeader(IPv4HDR *Header, ULONG SrcAddr, ULONG DstAddr, USHORT TotalLength)
{
    memset(Header, 0, IP_HDR_LEN);
    Header->Ipv4Preamble = 0x45;
    Header->TotalLength = htons(TotalLength);
    Header->TTL = 114;
    Header->Protocol = IPPROTO_TCP;
    Header->SrcAddr = SrcAddr;
    Header->DstAddr = DstAddr;
    Header->Checksum = IPv4Checksum(Header, IP_HDR_LEN);
}

ULONG LocalSeq = START_SEQ;
ULONG RemoteSeq = 0;

static DWORD
TunSockConnect(WINTUN_SESSION_HANDLE Session, SOCKADDR_IN *Address)
{
    BYTE *Packet;
    DWORD PacketSize;

    /* 1. Send SYN */
    PacketSize = IP_HDR_LEN + TCP_HDR_LEN;
    Packet = WintunAllocateSendPacket(Session, PacketSize);
    if (Packet) {
        TCPHDR *TcpHdr = (TCPHDR *)(Packet + IP_HDR_LEN);
        IPv4HDR *Ipv4Hdr = (IPv4HDR *)Packet;
        SetTcpHeader(TcpHdr, htons(LOCAL_PORT), Address->sin_port);
        SetIpHeader(Ipv4Hdr, inet_addr(LOCAL_ADDR), Address->sin_addr.s_addr, PacketSize);
        TcpHdr->Seq = htonl(LocalSeq);
        TcpHdr->Ack = htonl(0);
        TcpHdr->DO_Reserved_Flags |= htons(0x002); // Set SYN flag
        TcpHdr->Checksum = TcpChecksum(Packet, PacketSize);
        WintunSendPacket(Session, Packet);
        LocalSeq += 1;
        Log(WINTUN_LOG_INFO, L"Sent SYN packet");
    } else if (GetLastError() != ERROR_BUFFER_OVERFLOW) {
        return LogLastError(L"Packet write failed");
    }

    /* 2. Check SYN-ACK */

    while (TRUE) {
        Packet = WintunReceivePacket(Session, &PacketSize);
        if (!Packet) {
            DWORD LastError = GetLastError();
            if (LastError == ERROR_NO_MORE_ITEMS)
                continue;
            LogError(L"Packet read failed", LastError);
            return LastError;
        }
        TCPHDR *TcpHdr = (TCPHDR *)(Packet + IP_HDR_LEN);
        IPv4HDR *Ipv4Hdr = (IPv4HDR *)Packet;
        if (Ipv4Hdr->Ipv4Preamble == 0x45 && Ipv4Hdr->Protocol == IPPROTO_TCP) {
            if (TcpHdr->SrcPort == htons(REMOTE_PORT) && TcpHdr->DO_Reserved_Flags & htons(0x012) &&
                TcpHdr->Ack == htonl(LocalSeq)) {
                RemoteSeq = ntohl(TcpHdr->Seq) + 1;
                Log(WINTUN_LOG_INFO, L"Received SYN-ACK packet");
                WintunReleaseReceivePacket(Session, Packet);
                break;
            }
        }
        WintunReleaseReceivePacket(Session, Packet);
    }

    /* 3. Send ACK */
    PacketSize = IP_HDR_LEN + TCP_HDR_LEN;
    Packet = WintunAllocateSendPacket(Session, PacketSize);
    if (Packet) {
        TCPHDR *TcpHdr = (TCPHDR *)(Packet + IP_HDR_LEN);
        IPv4HDR *Ipv4Hdr = (IPv4HDR *)Packet;
        SetTcpHeader(TcpHdr, htons(LOCAL_PORT), Address->sin_port);
        SetIpHeader(Ipv4Hdr, inet_addr(LOCAL_ADDR), Address->sin_addr.s_addr, PacketSize);
        TcpHdr->Seq = htonl(LocalSeq);
        TcpHdr->Ack = htonl(RemoteSeq);
        TcpHdr->DO_Reserved_Flags |= htons(0x010); // Set ACK flag
        TcpHdr->Checksum = TcpChecksum(Packet, PacketSize);
        WintunSendPacket(Session, Packet);
        Log(WINTUN_LOG_INFO, L"Sent ACK packet");
    } else if (GetLastError() != ERROR_BUFFER_OVERFLOW) {
        return LogLastError(L"Packet write failed");
    }

    return ERROR_SUCCESS;
}

static DWORD
TunSockReset(WINTUN_SESSION_HANDLE Session, SOCKADDR_IN *Address)
{
    BYTE *Packet;
    DWORD PacketSize;

    /* 1. Send RST */
    PacketSize = IP_HDR_LEN + TCP_HDR_LEN;
    Packet = WintunAllocateSendPacket(Session, PacketSize);
    if (Packet) {
        TCPHDR *TcpHdr = (TCPHDR *)(Packet + IP_HDR_LEN);
        IPv4HDR *Ipv4Hdr = (IPv4HDR *)Packet;
        SetTcpHeader(TcpHdr, htons(LOCAL_PORT), Address->sin_port);
        SetIpHeader(Ipv4Hdr, inet_addr(LOCAL_ADDR), Address->sin_addr.s_addr, PacketSize);
        TcpHdr->Seq = htonl(LocalSeq);
        TcpHdr->Ack = htonl(RemoteSeq);
        TcpHdr->DO_Reserved_Flags |= htons(0x014); // Set RST and ACK
        TcpHdr->Checksum = TcpChecksum(Packet, PacketSize);
        WintunSendPacket(Session, Packet);
        Log(WINTUN_LOG_INFO, L"Sent RST packet");
    } else if (GetLastError() != ERROR_BUFFER_OVERFLOW) {
        return LogLastError(L"Packet write failed");
    }
    return ERROR_SUCCESS;
}

static DWORD
TunSockGetDynamicAddress(WINTUN_SESSION_HANDLE Session, SOCKADDR_IN *Address)
{
    const char *dhcp_discover =
        "\xff\xff\xff\xff\xff\xff\x00\x15\x5d\x32\xc7\x02\x08\x00\x45\xc0" \
        "\x01\x45\x00\x00\x40\x00\x40\x11\x38\xe9\x00\x00\x00\x00\xff\xff" \
        "\xff\xff\x00\x44\x00\x43\x01\x31\x52\x82\x01\x01\x06\x00\x7f\x8f" \
        "\x8d\x6e\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x15\x5d\x32\xc7\x02\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
        "\x00\x00\x00\x00\x00\x00\x63\x82\x53\x63\x35\x01\x01\x3d\x07\x01" \
        "\x00\x15\x5d\x32\xc7\x02\x37\x11\x01\x02\x06\x0c\x0f\x1a\x1c\x79" \
        "\x03\x21\x28\x29\x2a\x77\xf9\xfc\x11\x39\x02\x02\x40\x0c\x13\x73" \
        "\x68\x74\x2d\x56\x69\x72\x74\x75\x61\x6c\x2d\x4d\x61\x63\x68\x69" \
        "\x6e\x65\xff";

    BYTE *Packet;
    DWORD PacketSize;

    PacketSize = 325;
    Packet = WintunAllocateSendPacket(Session, PacketSize);
    memcpy(Packet, dhcp_discover + 14, PacketSize);
    WintunSendPacket(Session, Packet);

    return 0;
}

int __cdecl
main()
{
    HMODULE Wintun = InitializeWintun();
    if (!Wintun)
        return LogError(L"Failed to initialize Wintun", GetLastError());
    WintunSetLogger(ConsoleLogger);
    Log(WINTUN_LOG_INFO, L"Wintun library loaded");

    DWORD LastError;
    GUID ExampleGuid = { 0xdeadbabe, 0xcafe, 0xbeef, { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef } };
//    WINTUN_ADAPTER_HANDLE Adapter = WintunCreateAdapter(L"Demo", L"Example", &ExampleGuid);
    WINTUN_ADAPTER_HANDLE Adapter = WintunOpenAdapter(L"Demo");
    if (!Adapter)
    {
        LastError = GetLastError();
        LogError(L"Failed to open adapter", LastError);
        goto cleanupWintun;
    }

    DWORD Version = WintunGetRunningDriverVersion();
    Log(WINTUN_LOG_INFO, L"Wintun v%u.%u loaded", (Version >> 16) & 0xff, (Version >> 0) & 0xff);

    MIB_UNICASTIPADDRESS_ROW AddressRow;
    InitializeUnicastIpAddressEntry(&AddressRow);
    WintunGetAdapterLUID(Adapter, &AddressRow.InterfaceLuid);
    AddressRow.Address.Ipv4.sin_family = AF_INET;
    AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = htonl((10 << 24) | (0 << 16) | (0 << 8) | (1 << 0)); /* 10.0.0.1 */
    AddressRow.OnLinkPrefixLength = 24; /* This is a /24 network */
    AddressRow.DadState = IpDadStatePreferred;
    LastError = CreateUnicastIpAddressEntry(&AddressRow);
    if (LastError != ERROR_SUCCESS && LastError != ERROR_OBJECT_ALREADY_EXISTS)
    {
        LogError(L"Failed to set IP address", LastError);
        goto cleanupAdapter;
    }

    WINTUN_SESSION_HANDLE Session = WintunStartSession(Adapter, 0x400000);
    if (!Session)
    {
        LastError = LogLastError(L"Failed to create adapter");
        goto cleanupAdapter;
    }

    // Must sleep for 1 second here, or the server side will not reply
    Sleep(1000);

    SOCKADDR_IN Address;
    Address.sin_family = AF_INET;

//    for (int i = 0; i < 100; ++i) {
//        TunSockGetDynamicAddress(Session, &Address);
//        Log(WINTUN_LOG_INFO, L"DHCP Discover sent");
//        Sleep(1000);
//    }
//    if (TunSockGetDynamicAddress(Session, &Address) != 0)
//    {
//        LastError = LogLastError(L"Failed to get dynamic address");
//        goto cleanupSession;
//    }

    Address.sin_addr.s_addr = inet_addr(REMOTE_ADDR);
    Address.sin_port = htons(REMOTE_PORT);

    TunSockConnect(Session, &Address);
    TunSockReset(Session, &Address);

    cleanupSession:
    WintunEndSession(Session);
    cleanupAdapter:
    WintunCloseAdapter(Adapter);
    cleanupWintun:
    FreeLibrary(Wintun);
    return LastError;
}
