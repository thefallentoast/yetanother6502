#ifndef CONSTANTS_H
#define CONSTANTS_H
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

#endif /* CONSTANTS_H */