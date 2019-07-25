#define __STDC_FORMAT_MACROS

#include "panda/plugin.h"
#include "panda/rr/rr_log.h"

#include <stdio.h>

target_ulong dump_addr;
int dump_no = 1;
bool dump = false;

bool init_plugin(void *);
void uninit_plugin(void *);
int before_block_exec(CPUState *env, TranslationBlock *tb);
void dump_memory(void);

void dump_memory(void){
    char filename[13];
    snprintf(filename, 13, "statedump_%d", dump_no);
    FILE* out = fopen(filename, "wb");
    panda_memsavep(out);
    //char command1[1024];
    //snprintf(command1, sizeof(command1), "python2.7 /home/chris/Documents/phd/hw_symb/volatility/vol.py --file=./%s --profile=LinuxDebianx64  linux_dump_map -s 0x00000000006c6000 --dump-dir /home/chris/Documents/phd/hw_symb/panda/vol_proc_dumps/ --test %s",filename,filename);
    //printf("%s\n", command1);
    //if(system(command1) == -1) {
    //    printf("ERROR: Volatility command failed.");
    //    exit(1);   
    //}
        
    fclose(out);
    dump_no++;
}

int before_block_exec(CPUState *env, TranslationBlock *tb) {

    if(!dump && tb->pc == dump_addr) {
        dump_memory();
        printf("Dumped memory at instr %" PRIu64 "\n", rr_get_guest_instr_count());
        dump = true;
    } else {
        dump = false;
    }

    return 0;
}

bool init_plugin(void *self) {
    panda_cb pcb = { .before_block_exec = before_block_exec };
    panda_register_callback(self, PANDA_CB_BEFORE_BLOCK_EXEC, pcb);

    panda_arg_list *args = panda_get_args("hostapd_statedump");
    dump_addr = panda_parse_ulong_opt(args, "dump_addr", 0, "dump memory when execution hits this address");


    return true;
}

void uninit_plugin(void *self) {

}
