#ifndef CPU_H
#define CPU_H

#include <stdbool.h>  /****************************/
#include <string.h>   /* These   headers are used */
#include <stdlib.h>   /* practically everywhere   */
#include <stdio.h>    /****************************/
#include "crossplatform_functions.h"

#define guarantee(condition, error_message) if (!(condition)) { perror(error_message); return 1; }

typedef unsigned char  u8 ;
typedef signed   char  i8 ;
typedef unsigned short u16;
typedef signed   short i16;
typedef unsigned int   u32;

typedef struct RegFile_s {
    u8 A;
    u8 X;
    u8 Y;
    u8 IR;
    u8 P;

    u16 PC;
    u8  SP;
} RegFile;

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
    i8   offset;
    u8   reset_delay;
    u8   compare_operand;
    u16  access_address;
    u16  indirect_address;
    u16  old_pc;
    bool found_address;

    u8 aaa;
    u8 bbb;
    u8 cc;
} CPU;

typedef enum FLAGS_e : u8 {
    FLAGS_NEG = 0b10000000,
    FLAGS_OVR = 0b01000000,
    FLAGS_IGN = 0b00100000,
    FLAGS_BRK = 0b00010000,
    FLAGS_DEC = 0b00001000,
    FLAGS_IRE = 0b00000100,
    FLAGS_ZER = 0b00000010,
    FLAGS_CAR = 0b00000001
} FLAGS;

typedef enum ADDR_e : u8 {
    ADDR_X_IND = 0,
    ADDR_ZPG   = 1,
    ADDR_IMM   = 2,
    ADDR_ABS   = 3,
    ADDR_IND_Y = 4,
    ADDR_ZPG_X = 5,
    ADDR_ABS_Y = 6,
    ADDR_ABS_X = 7
} ADDR;

static const u8 branch_flag_by_index[] = {FLAGS_NEG, FLAGS_OVR, FLAGS_CAR, FLAGS_ZER};
static const u8 instruction_flag_by_index[] = {FLAGS_CAR, FLAGS_IRE, FLAGS_OVR, FLAGS_DEC};

/* External functions */
void CPU_reset(CPU* cpu, read_fn_ptr read_fn, write_fn_ptr write_fn);
void CPU_emulate(CPU* cpu);

#endif /* CPU_H */