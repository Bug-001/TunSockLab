//
// Created by SuHaitao on 2023/1/3.
//

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdint.h>

#include "tap-my.h"

int tap_my_write(tap_win32_overlapped_t *overlapped,
                    const void *buffer, unsigned long size)
{
    unsigned long write_size;
    BOOL result;
    DWORD error;

#ifdef TUN_ASYNCHRONOUS_WRITES
    result = GetOverlappedResult( overlapped->handle, &overlapped->write_overlapped,
                                  &write_size, FALSE);

    if (!result && GetLastError() == ERROR_IO_INCOMPLETE)
        WaitForSingleObject(overlapped->write_event, INFINITE);
#endif

    result = WriteFile(overlapped->handle, buffer, size,
                       &write_size, NULL);

#ifdef TUN_ASYNCHRONOUS_WRITES
    /* FIXME: we can't sensibly set write_size here, without waiting
     * for the IO to complete! Moreover, we can't return zero,
     * because that will disable receive on this interface, and we
     * also can't assume it will succeed and return the full size,
     * because that will result in the buffer being reclaimed while
     * the IO is in progress. */
#error Async writes are broken. Please disable TUN_ASYNCHRONOUS_WRITES.
#else /* !TUN_ASYNCHRONOUS_WRITES */
    if (!result) {
        error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            result = GetOverlappedResult(overlapped->handle,
                                         &overlapped->write_overlapped,
                                         &write_size, TRUE);
        }
    }
#endif

    if (!result) {
#ifdef DEBUG_TAP_WIN32
        LPTSTR msgbuf;
        error = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      &msgbuf, 0, NULL);
        fprintf(stderr, "Tap-Win32: Error WriteFile %d - %s\n", error, msgbuf);
        LocalFree(msgbuf);
#endif
        return 0;
    }

    return write_size;
}
