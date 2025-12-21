#include "cpu.h"

/* Flag modifying functions */
/* Sets NZ to appropriate values following a result*/
void CPU_set_NZ(CPU* cpu, u8 result) {
    cpu->r.P &= ~(FLAGS_NEG | FLAGS_ZER);
    cpu->r.P |= FLAGS_NEG & result; /* See footnote below the function */
    cpu->r.P |= result == 0 ? FLAGS_ZER : 0;
}
/* About the 6502's flags register:
 * Its flags register is formatted as
 * NV-BDIZC, which means the negative
 * flag is at the same place at the sign bit!
 * Essentially, &'ing it with the result
 * gives us the sign bit at the proper position already.
 */

/* CPU instruction functions */
void _CPU_LDA_IMM(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->r.A = cpu->read_fn(cpu->r.PC);
            CPU_set_NZ(cpu, cpu->r.A);
            cpu->r.PC++;
            cpu->cycle = 0; // Cycle 0 means "fetch instruction"
            break;
        }
            default:{
            cpu->cycle = 0;
            break; /* Do nothing, because the cycle is invalid */
        }
    }
}

void _CPU_LDA_ABS(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->access_address = cpu->read_fn(cpu->r.PC);
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 2: {
            cpu->access_address |= cpu->read_fn(cpu->r.PC) << 8;
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 3: {
            cpu->r.A = cpu->read_fn(cpu->access_address);
            CPU_set_NZ(cpu, cpu->r.A);
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_STA_ABS(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->access_address = cpu->read_fn(cpu->r.PC); // Load low byte
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 2: {
            cpu->access_address |= cpu->read_fn(cpu->r.PC) << 8;
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 3: {
            cpu->write_fn(cpu->access_address, cpu->r.A);
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_TAX(CPU* cpu) {
    cpu->r.X = cpu->r.A;
    CPU_set_NZ(cpu, cpu->r.X);
    cpu->cycle = 0;
}

void _CPU_CMP_logic(CPU* cpu, u8 cpu_register, u8 compare_operand) {
    u8 temp = cpu_register - compare_operand;
    cpu->r.P &= ~(FLAGS_ZER | FLAGS_NEG | FLAGS_CAR);
    cpu->r.P |= (temp == 0 ? FLAGS_ZER : 0) | (cpu_register >= compare_operand ? FLAGS_CAR : 0) | (temp & FLAGS_NEG);
}

void _CPU_CMP_IMM(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->compare_operand = cpu->read_fn(cpu->r.PC);
            cpu->r.PC++;
            _CPU_CMP_logic(cpu, cpu->r.A, cpu->compare_operand);
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_JMP_ABS(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->access_address = cpu->read_fn(cpu->r.PC);
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 2: {
            cpu->access_address |= cpu->read_fn(cpu->r.PC) << 8;
            cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 3: {
            cpu->r.PC = cpu->access_address;
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_BEQ(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            cpu->offset = (i8)cpu->read_fn(cpu->r.PC);
            cpu->old_pc = ++cpu->r.PC;
            cpu->cycle++;
            break;
        }
        case 2: {
            if (cpu->r.P & FLAGS_ZER) {
                cpu->r.PC = cpu->r.PC + cpu->offset;
                cpu->r.PC = (cpu->r.PC & 0xFF) + (cpu->old_pc & 0xFF00);
                cpu->cycle = ((cpu->r.PC & 0xFF00) != (cpu->old_pc & 0xFF00)) ? cpu->cycle+1 : 0;
            } else {
                cpu->cycle = 0;
            }
            break;
        }
        case 3: {
            cpu->r.PC = (cpu->r.PC & 0xFF) + ((cpu->r.PC + cpu->offset) & 0xFF00);
            cpu->cycle = 0;
            break;
        }
    }
}

void CPU_reset (CPU* cpu, read_fn_ptr read_fn, write_fn_ptr write_fn) {
    memset(cpu, 0, sizeof(*cpu));
    if (read_fn != NULL) {
        cpu->read_fn = read_fn;
    }
    if (write_fn != NULL) {
        cpu->write_fn = write_fn;
    }
    cpu->r.PC = 0xFFFC;
    cpu->r.P = FLAGS_IGN;
    cpu->reset_delay = 7;
    cpu->is_running = true;
}
/* This function executes 1 clock cycle of the CPU */
void CPU_emulate (CPU* cpu) {
    switch (cpu->reset_delay) {
        case 1: {
            // Fetch PC low byte
            cpu->r.PC = cpu->read_fn(0xFFFC);
            cpu->reset_delay--;
            return;
        }
        case 0: {
            // Fetch PC high byte
            cpu->r.PC |= (cpu->read_fn(0xFFFD) << 8);
            cpu->reset_delay = 0xFF;
            cpu->cycle = 0;
            return;
        }
        default: {
            cpu->reset_delay--;
            return;
        }
        case 0xFF: {
            break; // Should fall through
        }
    }
    if (cpu->cycle == 0) {
        cpu->r.IR = cpu->read_fn(cpu->r.PC);
        cpu->r.PC++;
        cpu->cycle++;
        cpu->instruction_count++;
        return;
    }
    
    switch (cpu->r.IR) {
        case 0xA9: { // LDA #
            _CPU_LDA_IMM(cpu);
            break;
        }
        case 0xAD: { // LDA abs
            _CPU_LDA_ABS(cpu);
            break;
        }
        case 0x8D: { // STA abs
            _CPU_STA_ABS(cpu);
            break;
        }
        case 0xAA: { // TAX impl
            _CPU_TAX(cpu);
            break;
        }
        case 0xC9: { // CMP #
            _CPU_CMP_IMM(cpu);
            break;
        }
        case 0x4C: { // JMP abs
            _CPU_JMP_ABS(cpu);
            break;
        }
        case 0xF0: { // BEQ rel
            _CPU_BEQ(cpu);
            break;
        }
        case 0x02: { // DBP (debug print)
            printf("A=%02X   X=%02X    Y=%02X    P=%08B    IR=%02X    SP=%02X    PC=%04X    IC=%08X\n", \
                   cpu->r.A, cpu->r.X, cpu->r.Y, cpu->r.P, cpu->r.IR, cpu->r.SP, cpu->r.PC, cpu->instruction_count);
            cpu->cycle = 0;
            break;
        }
        case 0x00: { // BRK
            cpu->is_running = false;
            break;
        }
        default: {
            printf("Illegal instruction $%02X at $%04X\n", cpu->r.IR, cpu->r.PC);
            cpu->is_running = false;
            break;
        }
    }
    
}