#include "qemu/osdep.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include "panda/plugin.h"
#include "tbb.pb.h"


extern "C" {
    bool init_plugin(void *);
    void uninit_plugin(void *);
}

extern int errno;

namespace {
const char * trace_file_name;
TBBBlocks blocks;

target_ulong last_pc = 0; 
unsigned int n = 0;

target_ulong asid;
target_ulong start_addr;

}

void write_entry(void) {

    TBBBlock *b = blocks.add_basic_blocks();
    b->set_address(last_pc);
    b->set_n(n);
    n = 1;
}


int before_block_exec_trace_tb(CPUState *env, TranslationBlock *tb) {

    // Don't bother logging memory read if performed by process we're not interested in. 
    // Nor if we only want to BBs within a specific memory range.
    if(! (!asid || panda_current_asid(env) == asid) ) return 0;

    // Don't trace system calls
    if (panda_in_kernel(env)) return 0;

    // reg defines are in target/i386/cpu.h
    // TODO use below for register logging complementary to QMP/gdb logging with avatar. 
    //CPUArchState *arch_env = (CPUArchState*)env->env_ptr;
    //printf(TARGET_FMT_lx "    ", arch_env->segs[R_FS].base);
    //printf(TARGET_FMT_lx "    ", arch_env->segs[R_CS].base);

    target_ulong pc = tb->pc;

    // If the trace begins at a specified breakpoint start_addr,
    // then we can set the ASID for later filtering
    if( start_addr && (pc == start_addr) ) {
        asid = panda_current_asid(env);
    }

    printf("%" PRIu64 "    ", rr_get_guest_instr_count());
    printf(TARGET_FMT_lx "    ", panda_current_asid(env));
    printf(TARGET_FMT_lx "\n", tb->pc);

    if ( (pc != last_pc || n == UINT_MAX ) && n > 0) {
        write_entry();
    }
    else {
        n++;
    }
    last_pc = pc;
    return 0;
}

bool init_plugin(void *self) {
panda_arg_list *args = panda_get_args("terrace_tbb");

    //TODO add arg for syscall mem logging

    trace_file_name = panda_parse_string_opt(args, "trace_file",
            "basic_blocks.bin", "File to store traced BB addresses");
    asid = panda_parse_ulong_opt(args, "asid", 0, 
            "The address space ID for the target process");
    start_addr = panda_parse_ulong_opt(args, "start_addr", 0, "known start/breakpoint address in record");

    //syscall_mem_file_name = panda_parse_string_opt(args, "trace_file",
     //       "basic_blocks.bin", "File to store traced BB addresses");
 
    panda_cb pcb;

    pcb.before_block_exec = before_block_exec_trace_tb;
    panda_register_callback(self, PANDA_CB_BEFORE_BLOCK_EXEC, pcb);



    return true;
}

void uninit_plugin(void *self) {
    write_entry();
    
    // write blocks to file
    std::ofstream file;

    file.open(trace_file_name, std::ios::out | std::ios::binary);

    if (!file) {
        std::cerr << "Failed to open " << trace_file_name << ":" << strerror(errno) << std::endl;
        exit(1);
    }
    blocks.SerializeToOstream(&file);
}
