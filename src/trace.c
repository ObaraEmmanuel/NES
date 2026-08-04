#include <string.h>
#include <stdio.h>
#include "cpu6502.h"
#include "mmu.h"
#include "utils.h"

static int is_official(uint8_t op_hex, Opcode opcode);

void print_cpu_trace(const c6502* ctx){
    static uint64_t traces = 0;
#if TRACER == 1
    // for use with the golden log
    if(traces >= 8991 && !PROFILE)
        quit(1);
#endif
    char address_str[28], opcode_hex_str[9];
    uint16_t addr, pc = ctx->pc, hi, lo;
    uint8_t opcode;
    opcode = read_mem(ctx->memory, pc++);
    const Instruction* instruction = &instructionLookup[opcode];
    const char* opcode_str = get_opcode(instruction->opcode);

    AddressMode mode = instruction->mode;
    if (instruction->opcode == JSR)
        mode = ABS;

    switch (mode) {
        case IMPL:
        case NONE:
        case SPEC:
            sprintf(opcode_hex_str, "%02X      ", opcode);
            sprintf(address_str, "                          ");
            break;
        case ACC:
            sprintf(opcode_hex_str, "%02X      ", opcode);
            sprintf(address_str, "A                         ");
            break;
        case REL: {
            int8_t offset = (int8_t)read_mem(ctx->memory, pc++);
            addr = pc + offset;
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, (uint8_t)offset);
            sprintf(address_str, "$%04X                     ", addr);
            break;
        }
        case IMT:
            lo = read_mem(ctx->memory, pc);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, lo);
            sprintf(address_str, "#$%02X                      ", lo);
            break;
        case ZPG:
            addr = read_mem(ctx->memory, pc);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, addr);
            sprintf(address_str, "$%02X = %02X                  ", addr, read_mem(ctx->memory, addr));
            break;
        case ZPG_X:
            addr = read_mem(ctx->memory, pc);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, addr);
            sprintf(
                address_str, "$%02X,X @ %02X = %02X           ",
                addr,
                (addr + ctx->x) & 0xFF,
                read_mem(ctx->memory, (addr + ctx->x) & 0xFF));
            break;
        case ZPG_Y:
            addr = read_mem(ctx->memory, pc);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, addr);
            sprintf(
                address_str, "$%02X,Y @ %02X = %02X           ",
                addr,
                (addr + ctx->y) & 0xFF,
                read_mem(ctx->memory, (addr + ctx->y) & 0xFF)
            );
            break;
        case ABS:
            lo = read_mem(ctx->memory, pc++);
            hi = read_mem(ctx->memory, pc);
            addr = (hi << 8) | lo;
            sprintf(opcode_hex_str, "%02X %02X %02X", opcode, lo, hi);
            if(instruction->opcode == JMP || instruction->opcode ==  JSR)
                sprintf(address_str, "$%04X                     ", addr);
            else
                sprintf(address_str, "$%04X = %02X                ", addr, read_mem(ctx->memory, addr));
            break;
        case ABS_X:
            lo = read_mem(ctx->memory, pc++);
            hi = read_mem(ctx->memory, pc);
            addr = (hi << 8) | lo;
            sprintf(opcode_hex_str, "%02X %02X %02X", opcode, lo, hi);
            sprintf(
                address_str, "$%04X,X @ %04X = %02X       ",
                addr,
                addr + ctx->x,
                read_mem(ctx->memory, addr + ctx->x)
            );
            break;
        case ABS_Y:
            lo = read_mem(ctx->memory, pc++);
            hi = read_mem(ctx->memory, pc);
            addr = (hi << 8) | lo;
            sprintf(opcode_hex_str, "%02X %02X %02X", opcode, lo, hi);
            sprintf(
                address_str, "$%04X,Y @ %04X = %02X       ",
                addr,
                (addr + ctx->y) & 0xFFFF,
                read_mem(ctx->memory, (addr + ctx->y) & 0xFFFF)
            );
            break;
        case IND:
            lo = read_mem(ctx->memory, pc++);
            hi = read_mem(ctx->memory, pc);
            addr = (hi << 8) | lo;
            sprintf(opcode_hex_str, "%02X %02X %02X", opcode, lo, hi);
            sprintf(
                address_str,
                "($%04X) = %04X            ",
                addr,
                ((uint16_t)read_mem(ctx->memory, (addr & 0xFF00) | (addr + 1) & 0xFF) << 8) | read_mem(ctx->memory, addr)
            );
            break;
        case IDX_IND:
            lo = read_mem(ctx->memory, pc);
            hi = (lo + ctx->x) & 0XFF;
            addr = ((uint16_t)read_mem(ctx->memory, (hi + 1) & 0xFF) << 8) | read_mem(ctx->memory, hi & 0xFF);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, lo);
            sprintf(
                address_str, "($%02X,X) @ %02X = %04X = %02X  ",
                lo,
                hi,
                addr,
                read_mem(ctx->memory, addr)
            );
            break;
        case IND_IDX:
            lo = read_mem(ctx->memory, pc);
            sprintf(opcode_hex_str, "%02X %02X   ", opcode, lo);
            hi = ((uint16_t)read_mem(ctx->memory, (lo + 1) & 0xFF) << 8) | read_mem(ctx->memory, lo & 0xFF);
            addr = hi + ctx->y;
            sprintf(
                address_str, "($%02X),Y = %04X @ %04X = %02X",
                lo,
                hi,
                addr,
                read_mem(ctx->memory, addr)
            );
            break;
    }

    PRINTF(
        "%04X  %s %s%s %s  A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%zu\n",
        ctx->pc,
        opcode_hex_str,
        ((instruction->opcode == NOP && instruction->mode != NONE) || !is_official(opcode, instruction->opcode)) ? "*": " ",
        opcode_str,
        address_str,
        ctx->ac,
        ctx->x,
        ctx->y,
        (ctx->sr & 0xef) | BIT_5,
        ctx->sp,
        ctx->t_cycles + 7
    );
    traces++;
}

char* get_opcode(Opcode opcode){
    switch (opcode) {
        case ADC:
            return "ADC";
        case AND:
            return "AND";
        case ASL:
            return "ASL";
        case BCC:
            return "BCC";
        case BCS:
            return "BCS";
        case BEQ:
            return "BEQ";
        case BIT:
            return "BIT";
        case BMI:
            return "BMI";
        case BNE:
            return "BNE";
        case BPL:
            return "BPL";
        case BRK:
            return "BRK";
        case BVC:
            return "BVC";
        case BVS:
            return "BVS";
        case CLC:
            return "CLC";
        case CLD:
            return "CLD";
        case CLI:
            return "CLI";
        case CLV:
            return "CLV";
        case CMP:
            return "CMP";
        case CPX:
            return "CPX";
        case CPY:
            return "CPY";
        case DEC:
            return "DEC";
        case DEX:
            return "DEX";
        case DEY:
            return "DEY";
        case EOR:
            return "EOR";
        case INC:
            return "INC";
        case INX:
            return "INX";
        case INY:
            return "INY";
        case JMP:
            return "JMP";
        case JSR:
            return "JSR";
        case LDA:
            return "LDA";
        case LDX:
            return "LDX";
        case LDY:
            return "LDY";
        case LSR:
            return "LSR";
        case NOP:
            return "NOP";
        case ORA:
            return "ORA";
        case PHA:
            return "PHA";
        case PHP:
            return "PHP";
        case PLA:
            return "PLA";
        case PLP:
            return "PLP";
        case ROL:
            return "ROL";
        case ROR:
            return "ROR";
        case RTI:
            return "RTI";
        case RTS:
            return "RTS";
        case SBC:
            return "SBC";
        case SEC:
            return "SEC";
        case SED:
            return "SED";
        case SEI:
            return "SEI";
        case STA:
            return "STA";
        case STX:
            return "STX";
        case STY:
            return "STY";
        case TAX:
            return "TAX";
        case TAY:
            return "TAY";
        case TSX:
            return "TSX";
        case TXA:
            return "TXA";
        case TXS:
            return "TXS";
        case TYA:
            return "TYA";

        // unofficial

        case ALR:
            return "ALR";
        case ANC:
            return "ANC";
        case ARR:
            return "ARR";
        case AXS:
            return "AXS";
        case LAX:
            return "LAX";
        case SAX:
            return "SAX";
        case DCP:
            return "DCP";
        case ISB:
            return "ISB";
        case RLA:
            return "RLA";
        case RRA:
            return "RRA";
        case SLO:
            return "SLO";
        case SRE:
            return "SRE";
        default:
            return "***";
    }
}

static int is_official(uint8_t op_hex, Opcode opcode){
    switch (opcode) {
        //case ALR:
        //case ANC:
        //case ARR:
        //case AXS:
        case LAX:
        case SAX:
        case DCP:
        case ISB:
        case RLA:
        case RRA:
        case SLO:
        case SRE:
            return 0;
        case SBC:
            if(op_hex == 0xEB)
                return 0;
        default:
            return 1;
    }
}

