//
// Created by SuHaitao on 2023/1/3.
//

#ifndef TUN_LAB_TAP_MY_H
#define TUN_LAB_TAP_MY_H

#include "tap-win32.h"

int tap_my_write(tap_win32_overlapped_t *overlapped,
                 const void *buffer, unsigned long size);

#endif //TUN_LAB_TAP_MY_H
