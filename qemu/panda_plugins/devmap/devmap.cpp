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
#include "pandalog.h"

}

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

struct Addr_Range {
    Addr_Range(target_ulong start, target_ulong size) :
            start(start), size(size), num_accesses(1), mmio(false) {
        end = start+size;
    }
    bool overlap(const Addr_Range& r, target_ulong pad = 0) {
        return start - pad <= r.end && r.start - pad <= end;
    }
    void merge(const Addr_Range& r) {
        this->start = std::min(this->start, r.start);
        this->end = std::max(this->end, r.end);
        this->size = this->end - this->start;
        if(this->start == r.start) this->phys_addr = r.phys_addr;
    }
    target_ulong start;
    target_ulong end;
    target_ulong size;
    target_phys_addr_t phys_addr;
    unsigned long num_accesses;
    bool mmio;
};

uint64_t bytes_read, bytes_written;
uint64_t num_reads, num_writes;
std::vector<Addr_Range> accesses;

void update_accesses(CPUState* env, target_ulong addr, target_ulong size) {
    if(pandalog && panda_is_io_memory(env, addr)) {
        Panda__AddrRange *range =
            (Panda__AddrRange *) malloc(sizeof(Panda__AddrRange));
        *range = PANDA__ADDR_RANGE__INIT;
        range->size = size;
        range->vstart = addr;
        range->pstart = cpu_get_phys_addr(env, addr);

        Panda__LogEntry ple = PANDA__LOG_ENTRY__INIT;
        ple.addr_range = range;
        pandalog_write_entry(&ple);
        free(range);
    }
#if 0
    Addr_Range r(addr, size);
    r.phys_addr = cpu_get_phys_addr(env, addr);
    bool broke = false;
    for(auto i = accesses.begin(); i != accesses.end(); i++) {
        if(i->overlap(r, 0x40)) {
           i->merge(r);
           i->num_accesses++;
           broke = true;
           break;
        }
    }
    if((r.mmio = panda_is_io_memory(env, addr))) {
        accesses.push_back(r);
    }
#endif
}

int mem_write_callback(CPUState *env, target_ulong pc, target_ulong addr,
                       target_ulong size, void *buf) {
    bytes_written += size;
    num_writes++;
    update_accesses(env, addr, size);
    return 1;
}

int mem_read_callback(CPUState *env, target_ulong pc, target_ulong addr,
                       target_ulong size, void *buf) {
    bytes_read += size;
    num_reads++;
    update_accesses(env, addr, size);
    return 1;
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

    return true;
}

void uninit_plugin(void *self) {
    printf("Memory statistics: %lu loads, %lu stores, %lu bytes read, %lu bytes written.\n",
        num_reads, num_writes, bytes_read, bytes_written
    );
    printf("Mem ranges:\n");
    for(auto i = accesses.begin(); i != accesses.end(); i++) {
        printf("vstart: 0x" TARGET_FMT_lx ", vend: 0x" TARGET_FMT_lx ", size: " TARGET_FMT_lu ", paddr: 0x" TARGET_FMT_plx ", IO: %d, #accesses: %lu\n",
                i->start, i->end, i->size, i->phys_addr, i->mmio, i->num_accesses);
    }
}
