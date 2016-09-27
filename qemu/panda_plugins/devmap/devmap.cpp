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
        start(start), size(size), end(start+size), num_accesses(1), mmio(false) {}
    bool overlap(const Addr_Range& r, target_ulong pad = 0) {
        return start - pad <= r.end && r.start - pad <= end;
    }
    void merge(const Addr_Range& r) {
        this->start = std::min(this->start, r.start);
        this->end = std::max(this->end, r.end);
        this->size = this->end - this->start;
    }
    target_ulong start;
    target_ulong size;
    target_ulong end;
    unsigned long num_accesses;
    bool mmio;
};

uint64_t bytes_read, bytes_written;
uint64_t num_reads, num_writes;
std::vector<Addr_Range> accesses;

void update_accesses(CPUState* env, target_ulong addr, target_ulong size) {
    Addr_Range r(addr, size);
    bool broke = false;
    for(auto i = accesses.begin(); i != accesses.end(); i++) {
        if(i->overlap(r)) {
           i->merge(r);
           i->num_accesses++;
           broke = true;
           break;
        }
    }
    if(!broke && (r.mmio = panda_is_io_memory(env, addr))) {
        accesses.push_back(r);
    }
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
        printf("start: 0x%lx, end: 0x%lx, size: %lu, IO: %d, #accesses: %lu\n",
                i->start, i->end, i->size, i->mmio, i->num_accesses);
    }
}
