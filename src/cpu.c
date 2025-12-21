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

void _CPU_branch_logic(CPU* cpu) {
    switch (cpu->cycle) {
        case 1:
            cpu->offset = (i8)cpu->read_fn(cpu->r.PC);
            cpu->old_pc = ++cpu->r.PC;
            cpu->cycle++;
            break;
        case 2:
            if ((bool)(cpu->r.P & branch_flag_by_index[(cpu->r.IR & 0xC0) >> 6]) ^ (bool)(cpu->r.IR & 0x40)) {
                cpu->r.PC = cpu->r.PC + cpu->offset;
                cpu->r.PC = (cpu->r.PC & 0xFF) + (cpu->old_pc & 0xFF00);
                cpu->cycle = ((cpu->r.PC & 0xFF00) != (cpu->old_pc & 0xFF00)) ? cpu->cycle+1 : 0;
            } else {
                cpu->cycle = 0;
            }
            break;
        case 3:
            cpu->r.PC = (cpu->r.PC & 0xFF) + ((cpu->r.PC + cpu->offset) & 0xFF00);
            cpu->cycle = 0;
            break;
    }
}

void _CPU_flags_logic(CPU* cpu) {
    // Only possible cpu->cycle is 1
    cpu->r.P &= ~(instruction_flag_by_index[(cpu->r.IR & 0xC0) >> 6]);
    cpu->r.P |= (bool)(cpu->r.IR & 0x20) ? instruction_flag_by_index[(cpu->r.IR & 0xC0) >> 6] : 0;
    cpu->cycle = 0;
    /*cpu->r.PC++;*/ /* This does not do any memory accesses and as such the PC is already pointing to the next instruction */
}

void _CPU_addressing_ABS(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: {
            // Low byte first
            cpu->access_address = cpu->read_fn(cpu->r.PC++);
            cpu->cycle++;
            break;
        }
        case 2: {
            // Next byte
            cpu->access_address |= cpu->read_fn(cpu->r.PC++) << 8;
            cpu->cycle++;
            break;
        }
    }
}

void _CPU_TAX(CPU* cpu) {
    cpu->r.X = cpu->r.A;
    CPU_set_NZ(cpu, cpu->r.X);
    cpu->cycle = 0;
}

void _CPU_TXA(CPU* cpu) {
    cpu->r.A = cpu->r.X;
    CPU_set_NZ(cpu, cpu->r.A);
    cpu->cycle = 0;
}

void _CPU_TAY(CPU* cpu) {
    cpu->r.Y = cpu->r.A;
    CPU_set_NZ(cpu, cpu->r.Y);
    cpu->cycle = 0;
}

void _CPU_TYA(CPU* cpu) {
    cpu->r.A = cpu->r.Y;
    CPU_set_NZ(cpu, cpu->r.A);
    cpu->cycle = 0;
}

void _CPU_LDA(CPU* cpu) {
    switch (cpu->bb) {
        case ADDR_IMM: {
            goto LDA_ADDR_IMM;
            break;
        }
        case ADDR_ABS: {
            goto LDA_ADDR_ABS;
            break;
        }
        default: {
            return;
        }
    }
    
    LDA_ADDR_IMM:
    cpu->r.A = cpu->read_fn(cpu->r.PC++);
    CPU_set_NZ(cpu, cpu->r.A);
    cpu->cycle = 0;
    return;

    LDA_ADDR_ABS:
    switch (cpu->cycle) {
        case 1:
        case 2: {
            _CPU_addressing_ABS(cpu); // This already increments cycle and the PC
            break;
        }
        case 3: {
            cpu->r.A = cpu->read_fn(cpu->access_address);
            cpu->cycle = 0;
            break;
        }
    }
    return;
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
    
    cpu->bb = (cpu->r.IR & 0b00011100) >> 2;

    switch (cpu->r.IR) {
        case 0xA9:   // LDA #
        case 0xAD: { // LDA abs
            _CPU_LDA(cpu);
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
        case 0x10:   // BPL rel
        case 0x30:   // BMI rel
        case 0x50:   // BVC rel
        case 0x70:   // BVS rel
        case 0x90:   // BCC rel
        case 0xB0:   // BCS rel
        case 0xD0:   // BNE rel
        case 0xF0: { // BEQ rel
            _CPU_branch_logic(cpu);
            break;
        }
        case 0x18: // CLC impl
        case 0x38: // SEC impl
        case 0x58: // CLI impl
        case 0x78: // SEI impl
        case 0xB8: // CLV impl /* note: there is no SEV, it's a pin, SOB */
        case 0xD8: // CLD impl
        case 0xF8: // SED impl
            _CPU_flags_logic(cpu);
            break;
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