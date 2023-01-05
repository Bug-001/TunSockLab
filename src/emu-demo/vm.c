//
// Created by SuHaitao on 2022/12/22.
//

#include "emu.h"

#include "string.h"

struct pkt_data {
    const char *name;
    const char *data;
    size_t len;
};

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

void vm_start()
{
    nic_buffer_len = 339;
    memcpy(nic_buffer, dhcp_discover, nic_buffer_len);
    vm_exit(VM_TX_INTR);
}