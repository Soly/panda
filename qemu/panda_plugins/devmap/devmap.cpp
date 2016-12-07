/* PANDABEGINCOMMENT
 *
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

extern "C" {

#include "config.h"
#include "qemu-common.h"
#include "monitor.h"
#include "cpu.h"
#include "disas.h"
#include "panda_plugin.h"
#include "panda_plugin_plugin.h"
#include "pandalog.h"

}

#include "../common/prog_point.h"
#include "../callstack_instr/callstack_instr_ext.h"
#include "../osi/osi_types.h"
#include "../osi/osi_ext.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <vector>

// These need to be extern "C" so that the ABI is compatible with
// QEMU/PANDA, which is written in C
extern "C" {

bool init_plugin(void *);
void uninit_plugin(void *);
int mem_write_callback(CPUState *env, target_ulong pc, target_ulong addr, target_ulong size, void *buf);
int mem_read_callback(CPUState *env, target_ulong pc, target_ulong addr, target_ulong size, void *buf);

}

void print_modules(CPUState *env);

uint64_t bytes_read, bytes_written;
uint64_t num_reads, num_writes;

void update_accesses(CPUState* env, target_ulong addr, target_ulong size,
                     char* type, void* data) {
    if(pandalog && panda_is_io_memory(env, addr)) {
        Panda__AddrRange *range =
            (Panda__AddrRange *) malloc(sizeof(Panda__AddrRange));
        *range = PANDA__ADDR_RANGE__INIT;
        range->size = size;
        range->vstart = addr;
        range->pstart = cpu_get_phys_addr(env, addr);
        range->type = type;
        if(size < 64) {
            range->has_data = true;
            range->data.len = size;
            range->data.data = (uint8_t*)data;
        }
        else range->has_data = false;

        Panda__CallStack *cs = pandalog_callstack_create();
        Panda__FunctionStack *fs = pandalog_functionstack_create();

        Panda__LogEntry ple = PANDA__LOG_ENTRY__INIT;
        ple.addr_range = range;
        ple.call_stack = cs;
        ple.function_stack = fs;
        pandalog_write_entry(&ple);
        free(range);
        pandalog_callstack_free(cs);
        pandalog_functionstack_free(fs);
    }
    print_modules(env);
}

int mem_write_callback(CPUState *env, target_ulong pc, target_ulong addr,
                       target_ulong size, void *buf) {
    bytes_written += size;
    num_writes++;
    update_accesses(env, addr, size, "w", buf);
    return 1;
}

int mem_read_callback(CPUState *env, target_ulong pc, target_ulong addr,
                       target_ulong size, void *buf) {
    bytes_read += size;
    num_reads++;
    update_accesses(env, addr, size, "r", buf);
    return 1;
}

void print_modules(CPUState *env) {
    OsiModules *kms = get_modules(env);
    if (kms == NULL) {
        // printf("No mapped kernel modules.\n");
    }
    else {
        printf("Kernel module list (%d modules):\n", kms->num);
        for (size_t i = 0; i < kms->num; i++)
            printf("\t0x" TARGET_FMT_lx "\t" TARGET_FMT_ld "\t%-24s %s\n", 
                    kms->module[i].base, kms->module[i].size, 
                    kms->module[i].name, kms->module[i].file);
    }
}

bool init_plugin(void *self) {
    panda_cb pcb;

    printf("Initializing plugin devmap\n");

    // Enable memory logging
    panda_enable_memcb();

    pcb.virt_mem_read = mem_read_callback;
    panda_register_callback(self, PANDA_CB_VIRT_MEM_READ, pcb);
    pcb.virt_mem_write = mem_write_callback;
    panda_register_callback(self, PANDA_CB_VIRT_MEM_WRITE, pcb);

    panda_require("callstack_instr");
    if (!init_callstack_instr_api()) return false;
    if(!init_osi_api()) return false;
    return true;
}

void uninit_plugin(void *self) {
    printf("Memory statistics: %lu loads, %lu stores, %lu bytes read, %lu bytes written.\n",
        num_reads, num_writes, bytes_read, bytes_written
    );
}
