#include "constants.h"
#include "cpu.h"

FILE *romFile = NULL;
u8 RAM[0x800]  = {0};
u8 ROM[0x8000] = {0};

/* For the CPU struct */
u8 cpu_read(u16 address) {
    if (address < 0x2000) {
        return RAM[address & 0x7FF];
    }
    if (address >= 0x8000) {
        return ROM[address & 0x7FFF];
    }
    return 0x00;
}
void cpu_write(u16 address, u8 data) {
    if (address < 0x2000) {
        RAM[address & 0x7FF] = data;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Not enough arguments!\n    Usage: ya6502 <rom file name or path>\n");
        return 1;
    }

    romFile = fopen(argv[1], "rb");
    guarantee(romFile != NULL, "Error opening file (does it exist?)");

    fseek(romFile, 0, SEEK_SET);
    size_t read_bytes = fread(ROM, 1, sizeof(ROM), romFile);
    guarantee(read_bytes > 0, "Error reading file (is it empty?)");
    fclose(romFile);

    CPU cpu;
    CPU_reset(&cpu, cpu_read, cpu_write);

    while (cpu.is_running) {
        CPU_emulate(&cpu);
        sleep_ms(1);
        //printf("A=%02X   X=%02X    Y=%02X    P=%08B    IR=%02X    SP=%02X    PC=%04X    IC=%08X\n", \
                cpu.r.A, cpu.r.X,  cpu.r.Y,  cpu.r.P,  cpu.r.IR,  cpu.r.SP,  cpu.r.PC,  cpu.instruction_count);
        //sleep(1);
    }
}