
/*
 cd panda/qemu/panda

 gcc -c   pandalog_reader.c  -g
 gcc -c   pandalog.pb-c.c -I .. -g
 gcc -c   pandalog_print.c  -g
 gcc -c   pandalog.c -I .. -D PANDALOG_READER  -g
 gcc -o pandalog_reader pandalog.o   pandalog.pb-c.o  pandalog_print.o  pandalog_reader.o -L/usr/local/lib -lprotobuf-c -g  -I .. -lz

*/

#define __STDC_FORMAT_MACROS
#define PADDING 0x0

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pandalog.h"
#include "pandalog_print.h"
#include <iostream>
#include <vector>
//#include <map>
//#include <string>


struct MemAccess {
    MemAccess(uint64_t size, uint64_t instr, uint64_t pc, uint64_t vstart, uint64_t pstart) :
            size(size), instr(instr), pc(pc), vstart(vstart), vend(vstart+size),
            pstart(pstart), pend(pstart+size), num_accesses(1), mmio(true) {}

    void pprint(void) {
        printf("size: %lu,", size);
        printf("instr: %lu,", instr);
        printf("pc: 0x%lx,", pc);
        printf("vstart: 0x%lx,", vstart);
        printf("vend: 0x%lx,", vend);
        printf("pstart: 0x%lx,", pstart);
        printf("pend: 0x%lx,", pend);
        printf("num_accesses: %lu,", num_accesses);
        printf("call_stack: %lu(", callstack.size());
        for(size_t i = 0; i < callstack.size(); i++) {
            printf("0x%lx", callstack[i]);
            if(i+1 >= callstack.size()) break;
            printf("->");
        }
        printf(")");
        printf("\n");
    }

    // IO access info
    uint64_t size;
    uint64_t vstart;
    uint64_t vend;
    uint64_t pstart;
    uint64_t pend;
    uint64_t num_accesses;
    bool mmio; // should always be true

    // other info from log
    uint64_t instr;
    uint64_t pc;
    std::vector<uint64_t> callstack;
};

std::vector<MemAccess> process_entries(const std::vector<Panda__LogEntry*>& entries,
                                       bool (*overlap)(MemAccess&, MemAccess&, int),
                                       void (*merge)(MemAccess&, MemAccess&));

bool overlap_by_prange(MemAccess& to, MemAccess& from, int pad);
void merge_by_prange(MemAccess& to, MemAccess& from);


int main (int argc, char **argv) {
    pandalog_open((const char *) argv[1], (const char*) "r");
    Panda__LogEntry *ple;
    std::vector<Panda__LogEntry*> entries;
    std::vector<MemAccess> logs;

    while (1) {
        ple = pandalog_read_entry();
        if (ple == (Panda__LogEntry *)1) {
            continue;
        }
        if (ple == NULL) {
            break;
        }
        // pprint_ple(ple);
        if (ple->addr_range && ple->instr) {
            auto rec = MemAccess(ple->addr_range->size, ple->instr, ple->pc,
                    ple->addr_range->vstart, ple->addr_range->pstart);
            if(ple->call_stack->n_addr)
                rec.callstack = std::vector<uint64_t>(ple->call_stack->addr,
                        ple->call_stack->addr + ple->call_stack->n_addr);
            logs.push_back(rec);
        }
    }

    // auto logs = process_entries(entries, overlap_by_prange, merge_by_prange);
    // printf("before merge: %lu\n", entries.size());
    // printf("after merge:  %lu\n", phys_ranges.size());
    // printf("ranges:\n\n");
    for(auto &i : logs) i.pprint();
    for(auto &i : entries)
        panda__log_entry__free_unpacked(i, NULL);
}

std::vector<MemAccess> process_entries(const std::vector<Panda__LogEntry*>& entries,
                                       bool (*overlap)(MemAccess&, MemAccess&, int),
                                       void (*merge)(MemAccess&, MemAccess&)) {
    bool broke;
    std::vector<MemAccess> r;
    for(auto &i : entries) {
        auto ar = MemAccess(i->addr_range->size, i->instr, i->pc,
                i->addr_range->vstart, i->addr_range->pstart);
        if(i->call_stack->n_addr)
            ar.callstack = std::vector<uint64_t>(i->call_stack->addr,
                    i->call_stack->addr + i->call_stack->n_addr);
        broke = false;
        /*
        for(auto &j : r) {
           if(overlap(j, ar, 0)) {
               merge(j, ar);
               j.num_accesses++;
               broke = true;
               break;
           }
        }
        if(!broke) r.push_back(ar);
        */
        r.push_back(ar);
    }
    return r;
}

// nearby physical addrs
bool overlap_by_prange(MemAccess& to, MemAccess& from, int pad) {
    return to.pstart - pad <= from.pend && from.pstart - pad <= to.pend;
}

// merge physical addr ranges
// TODO: figure out what to do with virtual addrs here.
// Do we even need them anymore?
void merge_by_prange(MemAccess& to, MemAccess& from) {
    to.pstart = std::min(to.pstart, from.pstart);
    to.pend = std::max(to.pend, from.pend);
    to.size = to.pend - to.pstart;
    // if(this->pstart == r.pstart) this->vstart = r.vstart;
}

// Temporal accesses
// Accesses that happen within pad intructions of eachother
bool overlap_by_instr(MemAccess& to, MemAccess& from, int pad) {
    return to.instr + pad >= from.instr && to.instr - pad <= from.instr;
}

// Not sure how to merge these without deferring to the same spatial tactic as
// before. That can be problematic here because the accesses can be far apart.
// Maybe some combination will work?
void merge_by_instr(MemAccess& to, MemAccess& from) {
    return;
}

// Nearby instructions
// e.g. loops usually handle sequential reads/writes
bool overlap_by_pc(MemAccess& to, MemAccess& from, int pad) {
    return to.pc + pad >= from.pc && to.pc - pad <= from.pc;
}

// See comment for merge_by_instr
void merge_by_pc(MemAccess& to, MemAccess& from) {
    return;
}
