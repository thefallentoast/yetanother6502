#include "cpu.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>

unsigned char to_print;
static struct termios oldt;

void keyboard_init(void) {
    struct termios newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO); // raw input
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void keyboard_restore(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int read_key(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;      // key pressed
    return 0;         // no key
}

int key_waiting(void) {
    fd_set fds;
    struct timeval tv = {0, 0};

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

FILE *romFile = NULL;
u8 RAM[0x800]  = {0};
u8 ROM[0x8000] = {0};

/* For the CPU struct */
u8 cpu_read(u16 address) {
    u8 output_value = 0;
    if (address == 0x5000) {
        return read_key();
    } else if (address == 0x5001) {
        return (u8)key_waiting() << 3;
    }
    if (address < 0x2000) {
        output_value = RAM[address & 0x7FF];
    }
    if (address >= 0x8000) {
        output_value = ROM[address & 0x7FFF];
    }
    //printf("A=%04X D=%02X R\n", address, output_value);
    return output_value;
}
void cpu_write(u16 address, u8 data) {
    if (address == 0x5000) {
        printf("%c", data); fflush(stdout);
        return;
    }
    if (address < 0x2000) {
        RAM[address & 0x7FF] = data;
    }
    //printf("A=%04X D=%02X W\n", address, data);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Not enough arguments!\n    Usage: ya6502 <rom file name or path>\n");
        return 1;
    }
    keyboard_init();

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
        //sleep_ms(2);
        //printf("A=%02X   X=%02X    Y=%02X    P=%08B    IR=%02X    SP=%02X    PC=%04X    IC=%08X ", \
                cpu.r.A, cpu.r.X,  cpu.r.Y,  cpu.r.P,  cpu.r.IR,  cpu.r.SP,  cpu.r.PC,  cpu.instruction_count);
        //printf("AA=%04X IA=%04X C=%02d OC=%04X \n", cpu.access_address, cpu.indirect_address, cpu.cycle, cpu.old_pc);
        //sleep(1);
    }
    keyboard_restore();
}