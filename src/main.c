#include <stdio.h>
#include <stdlib.h>
#include "constants.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Not enough arguments!\n    Usage: ya6502 <rom file name or path>\n");
        return 1;
    }
    RegFile r = {0};
    
    u8 RAM[0x800] =  {0};
    u8 ROM[0x8000] = {0};

    FILE *romFile = fopen(argv[1], "r+");
    guarantee(romFile != NULL, "Error opening file (does it exist?)");

    fseek(romFile, 0, SEEK_SET);
    size_t read_bytes = fread(ROM, 1, sizeof(ROM), romFile);
    guarantee(read_bytes > 0, "Error reading file (is it empty?)");

    printf("0x%02X 0x%02X 0x%02X\n", ROM[0], ROM[1], ROM[2]);
}