#ifndef CPU_H
#define CPU_H
#include "constants.h"

typedef u8   (*read_fn_ptr)(u16 address);
typedef void (*write_fn_ptr)(u16 address, u8 data);

typedef struct CPU_s {
    RegFile r;

    read_fn_ptr read_fn;  /* u8 read_fn(u16 address) */
    write_fn_ptr write_fn; /* void write_fn(u16 address, u8 data) */ 
    u8 cycle;
    bool is_running;
    u32 instruction_count;

    /* INTERNAL */
    i8  offset;
    u8  reset_delay;
    u8  compare_operand;
    u16 access_address;
    u16 indirect_address;
    u16 old_pc;
} CPU;

/* External functions */
void CPU_reset(CPU* cpu, read_fn_ptr read_fn, write_fn_ptr write_fn);
void CPU_emulate(CPU* cpu);

#endif /* CPU_H */