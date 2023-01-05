//
// Created by SuHaitao on 2022/12/22.
//

#ifndef TUN_LAB_TAP_WIN32_H
#define TUN_LAB_TAP_WIN32_H

#include "../../../../../mingw64/include/windows.h"
#include "../../../../../mingw64/include/winioctl.h"
#include "../../../../../mingw64/include/stdio.h"
#include "../../../../../mingw64/lib/gcc/x86_64-w64-mingw32/12.2.0/include/stdint.h"

#define TUN_BUFFER_SIZE 1560
#define TUN_MAX_BUFFER_COUNT 32

/*
 * The data member "buffer" must be the first element in the tun_buffer
 * structure. See the function, tap_win32_free_buffer.
 */
typedef struct tun_buffer_s {
    unsigned char buffer [TUN_BUFFER_SIZE];
    unsigned long read_size;
    struct tun_buffer_s* next;
} tun_buffer_t;

typedef struct tap_win32_overlapped {
    HANDLE handle;
    HANDLE read_event;
    HANDLE write_event;
    HANDLE output_queue_semaphore;
    HANDLE free_list_semaphore;
    HANDLE tap_semaphore;
    CRITICAL_SECTION output_queue_cs;
    CRITICAL_SECTION free_list_cs;
    OVERLAPPED read_overlapped;
    OVERLAPPED write_overlapped;
    tun_buffer_t buffers[TUN_MAX_BUFFER_COUNT];
    tun_buffer_t* free_list;
    tun_buffer_t* output_queue_front;
    tun_buffer_t* output_queue_back;
} tap_win32_overlapped_t;


int tap_win32_open(tap_win32_overlapped_t **phandle,
                          const char *preferred_name);

int tap_win32_read(tap_win32_overlapped_t *overlapped,
                          uint8_t **pbuf, int max_size);

int tap_win32_write(tap_win32_overlapped_t *overlapped,
                    const void *buffer, unsigned long size);

#endif //TUN_LAB_TAP_WIN32_H
