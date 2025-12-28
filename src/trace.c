#include <string.h>

#include "cpu6502.h"
#include "utils.h"

static void get_opcode(char* out, Opcode opcode);
static int is_official(uint8_t op_hex, Opcode opcode);

void print_cpu_trace(const c6502* ctx){
    static uint64_t traces = 0;
#if TRACER == 1
    // for use with the golden log
    if(traces >= 8991 && !PROFILE)
        quit(1);
#endif
    char opcode_str[4], address_str[28], opcode_hex_str[9];
    uint16_t addr, pc = ctx->pc, hi, lo;
    uint8_t opcode;
    opcode = read_mem(ctx->memory, pc++);
    const Instruction* instruction = &instructionLookup[opcode];
    get_opcode(opcode_str, instruction->opcode);

    switch (instruction->mode) {
        case IMPL:
        case NONE:
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
        ctx->sr,
        ctx->sp,
        ctx->t_cycles + 6
    );
    traces++;
}

static void get_opcode(char* out, Opcode opcode){
    switch (opcode) {
        case ADC:
            strcpy(out, "ADC");
            break;
        case AND:
            strcpy(out, "AND");
            break;
        case ASL:
            strcpy(out, "ASL");
            break;
        case BCC:
            strcpy(out, "BCC");
            break;
        case BCS:
            strcpy(out, "BCS");
            break;
        case BEQ:
            strcpy(out, "BEQ");
            break;
        case BIT:
            strcpy(out, "BIT");
            break;
        case BMI:
            strcpy(out, "BMI");
            break;
        case BNE:
            strcpy(out, "BNE");
            break;
        case BPL:
            strcpy(out, "BPL");
            break;
        case BRK:
            strcpy(out, "BRK");
            break;
        case BVC:
            strcpy(out, "BVC");
            break;
        case BVS:
            strcpy(out, "BVS");
            break;
        case CLC:
            strcpy(out, "CLC");
            break;
        case CLD:
            strcpy(out, "CLD");
            break;
        case CLI:
            strcpy(out, "CLI");
            break;
        case CLV:
            strcpy(out, "CLV");
            break;
        case CMP:
            strcpy(out, "CMP");
            break;
        case CPX:
            strcpy(out, "CPX");
            break;
        case CPY:
            strcpy(out, "CPY");
            break;
        case DEC:
            strcpy(out, "DEC");
            break;
        case DEX:
            strcpy(out, "DEX");
            break;
        case DEY:
            strcpy(out, "DEY");
            break;
        case EOR:
            strcpy(out, "EOR");
            break;
        case INC:
            strcpy(out, "INC");
            break;
        case INX:
            strcpy(out, "INX");
            break;
        case INY:
            strcpy(out, "INY");
            break;
        case JMP:
            strcpy(out, "JMP");
            break;
        case JSR:
            strcpy(out, "JSR");
            break;
        case LDA:
            strcpy(out, "LDA");
            break;
        case LDX:
            strcpy(out, "LDX");
            break;
        case LDY:
            strcpy(out, "LDY");
            break;
        case LSR:
            strcpy(out, "LSR");
            break;
        case NOP:
            strcpy(out, "NOP");
            break;
        case ORA:
            strcpy(out, "ORA");
            break;
        case PHA:
            strcpy(out, "PHA");
            break;
        case PHP:
            strcpy(out, "PHP");
            break;
        case PLA:
            strcpy(out, "PLA");
            break;
        case PLP:
            strcpy(out, "PLP");
            break;
        case ROL:
            strcpy(out, "ROL");
            break;
        case ROR:
            strcpy(out, "ROR");
            break;
        case RTI:
            strcpy(out, "RTI");
            break;
        case RTS:
            strcpy(out, "RTS");
            break;
        case SBC:
            strcpy(out, "SBC");
            break;
        case SEC:
            strcpy(out, "SEC");
            break;
        case SED:
            strcpy(out, "SED");
            break;
        case SEI:
            strcpy(out, "SEI");
            break;
        case STA:
            strcpy(out, "STA");
            break;
        case STX:
            strcpy(out, "STX");
            break;
        case STY:
            strcpy(out, "STY");
            break;
        case TAX:
            strcpy(out, "TAX");
            break;
        case TAY:
            strcpy(out, "TAY");
            break;
        case TSX:
            strcpy(out, "TSX");
            break;
        case TXA:
            strcpy(out, "TXA");
            break;
        case TXS:
            strcpy(out, "TXS");
            break;
        case TYA:
            strcpy(out, "TYA");
            break;

        // unofficial

        case ALR:
            strcpy(out, "ALR");
            break;
        case ANC:
            strcpy(out, "ANC");
            break;
        case ARR:
            strcpy(out, "ARR");
            break;
        case AXS:
            strcpy(out, "AXS");
            break;
        case LAX:
            strcpy(out, "LAX");
            break;
        case SAX:
            strcpy(out, "SAX");
            break;
        case DCP:
            strcpy(out, "DCP");
            break;
        case ISB:
            strcpy(out, "ISB");
            break;
        case RLA:
            strcpy(out, "RLA");
            break;
        case RRA:
            strcpy(out, "RRA");
            break;
        case SLO:
            strcpy(out, "SLO");
            break;
        case SRE:
            strcpy(out, "SRE");
            break;
        default:
            break;
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

