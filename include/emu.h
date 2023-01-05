//
// Created by SuHaitao on 2022/12/22.
//

#ifndef TUN_LAB_EMU_H
#define TUN_LAB_EMU_H

enum vm_exit_cause {
    VM_NONE,
    VM_TX_INTR
};

void vm_start();
void vm_exit(enum vm_exit_cause cause);

extern char *nic_buffer;
extern long nic_buffer_len;

#endif //TUN_LAB_EMU_H
