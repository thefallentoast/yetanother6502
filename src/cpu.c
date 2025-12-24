#include "cpu.h"
#define _EMULATE_W65C02S

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

void _CPU_push_to_stack(CPU* cpu, u8 value) {
    cpu->write_fn(0x100 + cpu->r.SP--, value);
}

u8 _CPU_pull_from_stack(CPU* cpu) {
    return cpu->read_fn(0x100 + ++cpu->r.SP);
}

void _CPU_branch_logic(CPU* cpu) {
    switch (cpu->cycle) {
        case 1:
            cpu->offset = (i8)cpu->read_fn(cpu->r.PC);
            cpu->old_pc = ++cpu->r.PC;
            cpu->cycle++;
            break;
        case 2:
            if ((bool)(cpu->r.P & branch_flag_by_index[(cpu->r.IR & 0xC0) >> 6]) ^ !(bool)(cpu->r.IR & 0x20)) {
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

void _CPU_addressing_IMM(CPU* cpu) {
    // Imm is the simplest.
    cpu->access_address = cpu->r.PC++;
    cpu->found_address = true;
    cpu->cycle = 0x80;
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
            cpu->cycle = 0x80;
            cpu->found_address = true;
            break;
        }
    }
}

void _CPU_addressing_ZPG(CPU* cpu) {
    // Only possible CPU cycle is 1
    // Low byte first
    cpu->access_address = cpu->read_fn(cpu->r.PC++);
    cpu->cycle = 0x80;
    cpu->found_address = true;
}

void _CPU_addressing_X_IND(CPU* cpu) { // (ZPG,X)
    switch (cpu->cycle) {
        case 1: {
            cpu->indirect_address = cpu->read_fn(cpu->r.PC++);
            cpu->cycle++;
            break;
        }
        case 2: {
            cpu->indirect_address = (u8)(cpu->indirect_address + cpu->r.X);
            cpu->cycle++;
            break;
        }
        case 3: {
            cpu->access_address = cpu->read_fn(cpu->indirect_address); cpu->indirect_address = (u8)(cpu->indirect_address + 1);
            cpu->cycle++;
            break;
        }
        case 4: {
            cpu->access_address |= cpu->read_fn(cpu->indirect_address) << 8;
            cpu->cycle = 0x80;
            cpu->found_address = true;
            break;
        }
    }
}

void _CPU_addressing_ABS_Y(CPU* cpu) { // abs,y
    switch (cpu->cycle) {
        case 1: { // Low byte
            cpu->access_address = cpu->read_fn(cpu->r.PC++);
            cpu->cycle++;
            break;
        }
        case 2: { // High byte
            cpu->access_address |= cpu->read_fn(cpu->r.PC++) << 8;
            cpu->cycle++;
            break;
        }
        case 3: { // Add
            cpu->access_address += cpu->r.Y;
            cpu->cycle = 0x80;
            cpu->found_address = true;
            break;
        }
    }
}

void _CPU_stack_manipulation(CPU* cpu) {
    // Very simple
    switch((cpu->r.IR & 0x60) >> 5) {
        case 0: { // Push P
            _CPU_push_to_stack(cpu, cpu->r.P | FLAGS_BRK);
            break;
        }
        case 1: { // Pull P
            cpu->r.P = _CPU_pull_from_stack(cpu) & ~FLAGS_BRK;
            break;    
        }
        case 2: { // Push A
            _CPU_push_to_stack(cpu, cpu->r.A);
            break;
        }
        case 3: { // Pull A
            cpu->r.A = _CPU_pull_from_stack(cpu);
            break;
        }
        #ifdef _EMULATE_W65C02S
            // TODO: add the PHX/PLX/PHY/PLY
        #endif
    }
    cpu->cycle = 0;
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
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto LDA_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
            default: {
                return;
            }
        }
        return;
    }
    LDA_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A = cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
}

void _CPU_STA(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                // STA does not have an # addressing mode, so it's a 2-byte NOP.
                cpu->r.PC++;
                cpu->cycle = 0;
                return;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
            default: {
                // Simply return
                return;
            }
        }
        return;
    }
    cpu->write_fn(cpu->access_address, cpu->r.A);
    cpu->cycle = 0;
}

void _CPU_LDX(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case 0: { // LDX is special(tm) in relation to its IMM
                _CPU_addressing_IMM(cpu); goto LDX_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    LDX_LOGIC:
        CPU_set_NZ(cpu, cpu->r.X = cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
}

void _CPU_STX(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                // STX does not have an # addressing mode, so it's a 2-byte NOP.
                cpu->r.PC++;
                cpu->cycle = 0;
                return;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
            default: {
                // Simply return
                return;
            }
        }
        return;
    }
    cpu->write_fn(cpu->access_address, cpu->r.X);
    cpu->cycle = 0;
}

void _CPU_LDY(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case 0: {
                _CPU_addressing_IMM(cpu); goto LDY_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
        }
        return;
    }
    LDY_LOGIC:
        CPU_set_NZ(cpu, cpu->r.Y = cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
}

void _CPU_STY(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                // STY does not have an # addressing mode, so it's a 2-byte NOP.
                cpu->r.PC++;
                cpu->cycle = 0;
                return;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            default: {
                // Simply return
                return;
            }
        }
        return;
    }
    cpu->write_fn(cpu->access_address, cpu->r.Y);
    cpu->cycle = 0;
}

void _CPU_CMP_logic(CPU* cpu, u8 cpu_register, u8 compare_operand) {
    u8 temp = cpu_register - compare_operand;
    cpu->r.P &= ~(FLAGS_ZER | FLAGS_NEG | FLAGS_CAR);
    cpu->r.P |= (temp == 0 ? FLAGS_ZER : 0) | (cpu_register >= compare_operand ? FLAGS_CAR : 0) | (temp & FLAGS_NEG);
}

void _CPU_CMP(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto CMP_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    CMP_LOGIC:
        _CPU_CMP_logic(cpu, cpu->r.A, cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_CPX(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto CPX_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
        }
        return;
    }
    CPX_LOGIC:
        _CPU_CMP_logic(cpu, cpu->r.X, cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_CPY(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto CPY_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
        }
        return;
    }
    CPY_LOGIC:
        _CPU_CMP_logic(cpu, cpu->r.Y, cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_BIT(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
        }
        return;
    }
    // Note: compare_operand is being used as operand here
    cpu->compare_operand = cpu->read_fn(cpu->access_address);
    CPU_set_NZ(cpu, cpu->compare_operand & cpu->r.A);
    cpu->r.P &= ~(FLAGS_NEG | FLAGS_OVR);
    cpu->r.P |= (cpu->compare_operand & (FLAGS_NEG | FLAGS_OVR));
    cpu->cycle = 0;
    return;
}

void _CPU_AND(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto AND_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    AND_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A &= cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_ORA(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto ORA_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    ORA_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A |= cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_EOR(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto EOR_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    EOR_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A ^= cpu->read_fn(cpu->access_address));
        cpu->cycle = 0;
        return;
}

void _CPU_ADC(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto ADC_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    ADC_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A += cpu->read_fn(cpu->access_address) + (cpu->r.P & FLAGS_CAR));
        cpu->cycle = 0;
        return;
}

void _CPU_SBC(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                _CPU_addressing_IMM(cpu); goto SBC_LOGIC; /* IMM is the only one that doesn't do any memory accesses 
                                                            * and as such happens in 2 cycles (fetch opcode + fetch operand), 
                                                            * instead of >3 (fetch opcode + 1 or 2 fetch address + 1 to 3 fetch 
                                                            * operand in memory) */
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    SBC_LOGIC:
        CPU_set_NZ(cpu, cpu->r.A = cpu->r.A - cpu->read_fn(cpu->access_address) - (1 - (cpu->r.P & FLAGS_CAR)));
        cpu->cycle = 0;
        return;
}

void _CPU_ASL(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                // This skips addressing entirely and just does the operation on A.
                cpu->r.A <<= 1;
                cpu->cycle = 0;
                break;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    
    switch (cpu->cycle) { // this time it starts at 0x80
        case 0x80: {
            // Read the operand
            // Note: compare_operand is being used as operand here
            cpu->compare_operand = cpu->read_fn(cpu->access_address);
            cpu->cycle++;
            break;
        }
        case 0x81: {
            // Write back the original operand and perform the shift
            cpu->write_fn(cpu->access_address, cpu->compare_operand);
            cpu->compare_operand <<= 1;
            cpu->cycle++;
            break;
        }
        case 0x82: {
            // Write the new operand
            cpu->write_fn(cpu->access_address, cpu->compare_operand);
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_ROL(CPU* cpu) {
    if (!cpu->found_address) {
        switch (cpu->bbb) {
            case ADDR_X_IND: {
                _CPU_addressing_X_IND(cpu); break;
            }
            case ADDR_ZPG: {
                _CPU_addressing_ZPG(cpu); break;
            }
            case ADDR_IMM: {
                // This skips addressing entirely and just does the operation on A.
                cpu->r.A <<= 1;
                cpu->cycle = 0;
                break;
            }
            case ADDR_ABS: {
                _CPU_addressing_ABS(cpu); break;
            }
            case ADDR_ABS_Y: {
                _CPU_addressing_ABS_Y(cpu); break;
            }
        }
        return;
    }
    
    switch (cpu->cycle) { // this time it starts at 0x80
        case 0x80: {
            // Read the operand
            // Note: compare_operand is being used as operand here
            cpu->compare_operand = cpu->read_fn(cpu->access_address);
            cpu->cycle++;
            break;
        }
        case 0x81: {
            // Write back the original operand and perform the shift
            cpu->write_fn(cpu->access_address, cpu->compare_operand);
            u8 previous_carry = cpu->r.P & FLAGS_CAR; // No bit shift is needed as FLAGS_CAR = 1
            cpu->r.P &= ~FLAGS_CAR;
            cpu->r.P |= (i8)cpu->compare_operand < 0 ? FLAGS_CAR : 0;
            cpu->compare_operand <<= 1;
            cpu->compare_operand |= previous_carry;
            cpu->cycle++;
            break;
        }
        case 0x82: {
            // Write the new operand
            cpu->write_fn(cpu->access_address, cpu->compare_operand);
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

void _CPU_JMP_IND(CPU* cpu) {
    #ifdef _EMULATE_W65C02S
    switch (cpu->cycle) {
        case 1: {
            cpu->indirect_address = cpu->read_fn(cpu->r.PC); // Low byte
            cpu->old_pc = cpu->r.PC++;
            cpu->cycle++;
            break;
        }
        case 2: {
            if ((cpu->old_pc & 0xFF) == 0xFF) {
                // Page boundary will be crossed
                cpu->cycle = 3;
                break;
            }
            // Else, simply get the next byte and skip
            cpu->indirect_address |= cpu->read_fn(cpu->r.PC++) << 8;
            cpu->cycle = 4;
            break;
        }
        case 3: {
            // Page boundary was crossed.
            cpu->indirect_address |= cpu->read_fn(cpu->r.PC++) << 8; // We have to "fake" the addition in order to maintain cycle accuracy
            cpu->cycle++;
            break;
        }
        case 4: {
            cpu->access_address = cpu->read_fn(cpu->indirect_address++);
            cpu->cycle++;
            break;
        }
        case 5: {
            cpu->access_address |= cpu->read_fn(cpu->indirect_address) << 8;
            cpu->r.PC = cpu->access_address;
            cpu->cycle = 0;
        }
    }
    #else
    #endif
}

void _CPU_JSR(CPU* cpu) {
    switch (cpu->cycle) {
        case 1: { // Note: old_pc is being used as a new_pc here
            cpu->old_pc = cpu->read_fn(cpu->r.PC++);
            cpu->cycle++;
            break;
        }
        case 2: {
            cpu->old_pc |= cpu->read_fn(cpu->r.PC++) << 8;
            cpu->cycle++;
            break;
        }
        case 3: { // Note: access_address is being used as the value to push to stack
            cpu->access_address = cpu->r.PC - 1;
            _CPU_push_to_stack(cpu, (cpu->access_address & 0xFF00) >> 8);
            cpu->cycle++;
            break;
        }
        case 4: {
            _CPU_push_to_stack(cpu, cpu->access_address & 0xFF);
            cpu->cycle++;
            break;
        }
        case 5: {
            cpu->r.PC = cpu->old_pc;
            cpu->cycle = 0;
            break;
        }
    }
}

void _CPU_RTS(CPU* cpu) {
    switch(cpu->cycle) {
        case 1: { // Dummy read
            cpu->read_fn(cpu->r.SP); // I don't know if it's the SP being used or the PC
            cpu->cycle++;
            break;
        }
        case 2: { // Note: old_pc is being used as a new_pc here
            cpu->old_pc = _CPU_pull_from_stack(cpu);
            cpu->cycle++;
            break;
        }
        case 3: {
            cpu->old_pc |= _CPU_pull_from_stack(cpu) << 8;
            cpu->cycle++;
            break;
        }
        case 4: {
            // Just increment it because JSR pushes PC-1 for some reason
            cpu->r.PC = ++cpu->old_pc;
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
    cpu->r.PC = 0xFFFC; cpu->r.SP = 0xEA; // It should be a random value, so I chose $EA "randomly"
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
        cpu->found_address = false;

        // Compute octal triplet
        cpu->aaa = (cpu->r.IR & 0b11100000) >> 5;
        cpu->bbb = (cpu->r.IR & 0b00011100) >> 2;
        cpu->cc  = (cpu->r.IR & 0b00000011);

        return;
    }

    if (cpu->cc == 1) {
        // Instructions involving the accumulator.
        switch (cpu->aaa) {
            case 0: {
                _CPU_ORA(cpu);
                break;
            }
            case 1: {
                _CPU_AND(cpu);
                break;
            }
            case 2: {
                _CPU_EOR(cpu);
                break;
            }
            case 3: {
                _CPU_ADC(cpu);
                break;
            }
            case 4: {
                _CPU_STA(cpu);
                break;
            }
            case 5: {
                _CPU_LDA(cpu);
                break;
            }
            case 6: {
                _CPU_CMP(cpu);
                break;
            }
            case 7: {
                _CPU_SBC(cpu);
                break;
            }
            default:
                goto SPECIAL_INSTRUCTIONS;
        }
        return;
    } else if (cpu->cc == 2) {
        // For ASL/ROL/LSR/ROR
        switch (cpu->aaa) {
            case 0: {
                _CPU_ASL(cpu);
                break;
            }
            case 1: {
                _CPU_ROL(cpu);
                break;
            }
            default:
                goto SPECIAL_INSTRUCTIONS;
        }
        return;
    }
    
    SPECIAL_INSTRUCTIONS:
    switch (cpu->r.IR) {
        case 0xAA: { // TAX impl
            _CPU_TAX(cpu);
            break;
        }
        case 0x4C: { // JMP abs
            _CPU_JMP_ABS(cpu);
            break;
        }
        case 0x6C: { // JMP ind
            _CPU_JMP_IND(cpu);
            break;
        }
        case 0x20: { // JSR abs
            _CPU_JSR(cpu);
            break;
        }
        case 0x60: { // RTS impl
            _CPU_RTS(cpu);
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
        case 0x18:   // CLC impl
        case 0x38:   // SEC impl
        case 0x58:   // CLI impl
        case 0x78:   // SEI impl
        case 0xB8:   // CLV impl /* note: there is no SEV, it's a pin, SOB */
        case 0xD8:   // CLD impl
        case 0xF8: { // SED impl
            _CPU_flags_logic(cpu);
            break;
        }
        case 0x08:   // PHP impl
        case 0x28:   // PLP impl
        case 0x48:   // PHA impl
        case 0x68: { // PLA impl
            _CPU_stack_manipulation(cpu);
            break;
        }
        case 0xA2:   // LDX #
        case 0xA6:   // LDX zpg
        case 0xAE: { // LDX abs
            _CPU_LDX(cpu);
            break;
        }
        case 0x86:   // STX zpg
        case 0x8E:   // STX abs
        case 0x96: { // STX zpg,Y
            _CPU_STX(cpu);
            break;
        }
        case 0xA0:   // LDY #
        case 0xA4:   // LDY zpg
        case 0xAC: { // LDY abs
            _CPU_LDY(cpu);
            break;
        }
        case 0x84:   // STY zpg
        case 0x8C:   // STY abs
        case 0x94: { // STY zpg,X
            _CPU_STY(cpu);
            break;
        }
        case 0xE8: { // INX impl
            CPU_set_NZ(cpu, ++cpu->r.X);
            cpu->cycle = 0;
            break;
        }
        case 0xCA: { // DEX impl
            CPU_set_NZ(cpu, --cpu->r.X);
            cpu->cycle = 0;
            break;
        }
        case 0xC8: { // INY impl
            CPU_set_NZ(cpu, ++cpu->r.Y);
            cpu->cycle = 0;
            break;
        }
        case 0x88: { // DEY impl
            CPU_set_NZ(cpu, --cpu->r.Y);
            cpu->cycle = 0;
            break;
        }
        #ifdef _EMULATE_W65C02S
        case 0x1A: { // INC A
            CPU_set_NZ(cpu, ++cpu->r.A);
            cpu->cycle = 0;
            break;
        }
        case 0x3A: { // DEC A
            CPU_set_NZ(cpu, --cpu->r.A);
            cpu->cycle = 0;
            break;
        }
        #endif
        case 0xE0:   // CPX #
        case 0xE4:   // CPX zpg
        case 0xEC: { // CPX abs
            _CPU_CPX(cpu);
            break;
        }
        case 0xC0:   // CPY #
        case 0xC4:   // CPY zpg
        case 0xCC: { // CPY abs
            _CPU_CPY(cpu);
            break;
        }
        case 0x24:   // BIT zpg
        case 0x2C: { // BIT abs
            _CPU_BIT(cpu);
            break;
        }
        case 0xEA: { // NOP impl
            // We must do nothing for 1+1 cycles (fetch opcode + do nothing)
            cpu->cycle = 0;
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