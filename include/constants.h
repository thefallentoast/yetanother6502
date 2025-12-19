#ifndef CONSTANTS_H
#define CONSTANTS_H

#define guarantee(condition, error_message) if (!(condition)) { perror(error_message); return 1; }

typedef unsigned char  u8 ;
typedef signed   char  i8 ;
typedef unsigned short u16;
typedef signed   short i16;

typedef struct RegFile_s {
    u8 A;
    u8 X;
    u8 Y;
    u8 IR;

    u16 PC;
    u8  SP;
} RegFile;

#endif /* CONSTANTS_H */