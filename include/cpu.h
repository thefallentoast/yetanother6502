#ifndef CPU_H
#define CPU_H
#include "constants.h"

typedef u8  (*read_fn_ptr)(u16 address);
typedef u16 (*write_fn_ptr)(u16 address, u8 data);

typedef struct CPU_s {
    RegFile r;

    read_fn_ptr read_fn;  /* u8 read_fn(u16 address) */
    write_fn_ptr write_fn; /* void write_fn(u16 address, u8 data) */ 
    u8 cycle;

    /* INTERNAL */
} CPU;

void CPU_reset(CPU* cpu);
void CPU_emulate(CPU* cpu);

#endif /* CPU_H */