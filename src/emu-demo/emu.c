//
// Created by SuHaitao on 2022/12/22.
//

#include "emu.h"

#include "tap-win32.h"
#include "tap-my.h"

tap_win32_overlapped_t *handle;

wchar_t buf[256];

char *nic_buffer = NULL;
long nic_buffer_len = 0;

void LogError()
{
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
    printf("%ls", buf);
}

int __cdecl
main()
{
    const char *ifname = "Tap";  // FIXME: Could not be non-ASCII char

    if (tap_win32_open(&handle, ifname) < 0) {
        printf("tap: Could not open '%s'\n", ifname);
        LogError();
        return -1;
    }

    nic_buffer = malloc(4096);
    // TODO: start tap read thread

    vm_start();

    return 0;
}

int vm_exit(enum vm_exit_cause cause)
{
    switch (cause) {
        case VM_TX_INTR:
            return tap_win32_write(handle, nic_buffer, nic_buffer_len);
        case VM_STOP:
            exit(0);
        default:
            return -1;
    }
}