#include <string.h>
#include <stdlib.h>
#include "cpu.h"

void CPU_reset (CPU* cpu, read_fn_ptr read_fn, write_fn_ptr write_fn) {
    memset(cpu, 0, sizeof(cpu));
    cpu->read_fn = read_fn;
    cpu->write_fn = write_fn;
    cpu->r.PC = 0xFFFC;
}
void CPU_emulate (CPU* cpu) {

}