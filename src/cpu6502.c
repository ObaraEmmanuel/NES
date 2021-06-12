#include "cpu6502.h"
#include "utils.h"


static uint16_t get_address(c6502* ctx);
static uint16_t read_abs_address(Memory* mem, uint16_t offset);
static void set_ZN(c6502* ctx, uint8_t value);
static void fast_set_ZN(c6502* ctx, uint8_t value);
static uint8_t shift_l(c6502* ctx, uint8_t val);
static uint8_t shift_r(c6502* ctx, uint8_t val);
static uint8_t rot_l(c6502* ctx, uint8_t val);
static uint8_t rot_r(c6502* ctx, uint8_t val);
static void push(c6502* ctx, uint8_t value);
static void push_address(c6502* ctx, uint16_t address);
static uint8_t pop(c6502* ctx);
static uint16_t pop_address(c6502* ctx);
static void branch(c6502* ctx, uint16_t address, uint8_t mask, uint8_t predicate);
static uint8_t has_page_break(uint16_t addr1, uint16_t addr2);

void reset_cpu(c6502* cpu){
    cpu->ac = cpu->x = cpu->y = 0;
    cpu->cycles = 1;
    cpu->odd_cycle = 0;
    cpu->sr = 0x24;
    cpu->sp = 0xfd;
    cpu->pc = read_abs_address(cpu->memory, RESET_ADDRESS);
}

void interrupt(c6502* ctx, Interrupt interrupt){
    if((ctx->sr & INTERRUPT) && interrupt != NMI)
        return;

    uint16_t addr;

    switch (interrupt) {
        case NMI:
            addr = NMI_ADDRESS;
            break;
        case IRQ:
            addr = IRQ_ADDRESS;
            ctx->sr |= BREAK;
            break;
        case RSI:
            addr = RESET_ADDRESS;
            break;
        default:
            LOG(ERROR, "Unknown interrupt");
            return;
    }

    push_address(ctx, ctx->pc);
    push(ctx, ctx->sr);
    ctx->sr &= ~INTERRUPT;
    ctx->sr |= INTERRUPT;
    ctx->pc = read_abs_address(ctx->memory, addr);
}

void execute(c6502* ctx){
    ctx->odd_cycle ^= 1;
    ctx->t_cycles++;
    if(--ctx->cycles > 0){
        return;
    }
    if(TRACER)
        print_cpu_trace(ctx);
    uint8_t opcode = read_mem(ctx->memory, ctx->pc++);
    ctx->instruction = &instructionLookup[opcode];
    uint16_t address = get_address(ctx);
    ctx->cycles += cycleLookup[opcode];
    if(address > 0x4020 && address < 0x6000)
        LOG(DEBUG, "Expansion ROM address: $%04X, opcode: %02X", address, opcode);

    switch (ctx->instruction->opcode) {

        // Load and store opcodes

        case LDA:
            ctx->ac = read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);
            break;
        case LDX:
            ctx->x = read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->x);
            break;
        case LDY:
            ctx->y = read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->y);
            break;
        case STA:
            write_mem(ctx->memory, address, ctx->ac);
            break;
        case STY:
            write_mem(ctx->memory, address, ctx->y);
            break;
        case STX:
            write_mem(ctx->memory, address, ctx->x);
            break;

        // Register transfer opcodes

        case TAX:
            ctx->x = ctx->ac;
            set_ZN(ctx, ctx->x);
            break;
        case TAY:
            ctx->y = ctx->ac;
            set_ZN(ctx, ctx->y);
            break;
        case TXA:
            ctx->ac = ctx->x;
            set_ZN(ctx, ctx->ac);
            break;
        case TYA:
            ctx->ac = ctx->y;
            set_ZN(ctx, ctx->ac);
            break;

        // stack opcodes

        case TSX:
            ctx->x = ctx->sp;
            set_ZN(ctx, ctx->x);
            break;
        case TXS:
            ctx->sp = ctx->x;
            break;
        case PHA:
            push(ctx, ctx->ac);
            break;
        case PHP:
            // 6502 quirk bit 4 and 5 are always set by this instruction: see also BRK
            push(ctx, ctx->sr | BIT_4 | BIT_5);
            break;
        case PLA:
            ctx->ac = pop(ctx);
            set_ZN(ctx, ctx->ac);
            break;
        case PLP:
            // ignore bit 5 and 4
            ctx->sr &= (BIT_4 |BIT_5);
            ctx->sr |= pop(ctx) & ~(BIT_4 | BIT_5);
            break;

        // logical opcodes

        case AND:
            ctx->ac &= read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);
            break;
        case EOR:
            ctx->ac ^= read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);
            break;
        case ORA:
            ctx->ac |= read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);
            break;
        case BIT: {
            uint8_t opr = read_mem(ctx->memory, address);
            ctx->sr &= ~(NEGATIVE | OVERFLOW | ZERO);
            ctx->sr |= (!(opr & ctx->ac) ? ZERO: 0);
            ctx->sr |= (opr & (NEGATIVE | OVERFLOW));
            break;
        }

        // Arithmetic opcodes

        case ADC: {
            uint8_t opr = read_mem(ctx->memory, address);
            uint16_t sum = ctx->ac + opr + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLOW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY: 0);
            ctx->sr |= ((ctx->ac ^ sum) & (opr ^ sum) & 0x80) ? OVERFLOW: 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case SBC: {
            uint8_t opr = read_mem(ctx->memory, address);
            uint16_t diff = ctx->ac - opr - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLOW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~opr ^ diff) & 0x80) ? OVERFLOW: 0;
            ctx->ac = diff;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case CMP: {
            uint16_t diff = ctx->ac - read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case CPX: {
            uint16_t diff = ctx->x - read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0x100) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case CPY: {
            uint16_t diff = ctx->y - read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }

        // Increments and decrements opcodes

        case DEC:{
            uint8_t m = read_mem(ctx->memory, address) - 1;
            write_mem(ctx->memory, address, m);
            set_ZN(ctx, m);
            break;
        }
        case DEX:
            ctx->x--;
            set_ZN(ctx, ctx->x);
            break;
        case DEY:
            ctx->y--;
            set_ZN(ctx, ctx->y);
            break;
        case INC:{
            uint8_t m = read_mem(ctx->memory, address) + 1;
            write_mem(ctx->memory, address, m);
            set_ZN(ctx, m);
            break;
        }
        case INX:
            ctx->x++;
            set_ZN(ctx, ctx->x);
            break;
        case INY:
            ctx->y++;
            set_ZN(ctx, ctx->y);
            break;

        // shifts and rotations opcodes

        case ASL:
            if(ctx->instruction->mode == ACC) {
                ctx->ac = shift_l(ctx, ctx->ac);
            }else{
                write_mem(ctx->memory, address, shift_l(ctx, read_mem(ctx->memory, address)));
            }
            break;
        case LSR:
            if(ctx->instruction->mode == ACC) {
                ctx->ac = shift_r(ctx, ctx->ac);
            }else{
                write_mem(ctx->memory, address, shift_r(ctx, read_mem(ctx->memory, address)));
            }
            break;
        case ROL:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_l(ctx, ctx->ac);
            }else{
                write_mem(ctx->memory, address, rot_l(ctx, read_mem(ctx->memory, address)));
            }
            break;
        case ROR:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_r(ctx, ctx->ac);
            }else{
                write_mem(ctx->memory, address, rot_r(ctx, read_mem(ctx->memory, address)));
            }
            break;

        // jumps and calls

        case JMP:
            ctx->pc = address;
            break;
        case JSR:
            push_address(ctx, ctx->pc - 1);
            ctx->pc = address;
            break;
        case RTS:
            ctx->pc = pop_address(ctx) + 1;
            break;

        // branching opcodes

        case BCC:
            branch(ctx, address, CARRY, 0);
            break;
        case BCS:
            branch(ctx, address, CARRY, 1);
            break;
        case BEQ:
            branch(ctx, address, ZERO, 1);
            break;
        case BMI:
            branch(ctx, address, NEGATIVE, 1);
            break;
        case BNE:
            branch(ctx, address, ZERO, 0);
            break;
        case BPL:
            branch(ctx, address, NEGATIVE, 0);
            break;
        case BVC:
            branch(ctx, address, OVERFLOW, 0);
            break;
        case BVS:
            branch(ctx, address, OVERFLOW, 1);
            break;

        // status flag changes

        case CLC:
            ctx->sr &= ~CARRY;
            break;
        case CLD:
            ctx->sr &= ~DECIMAL;
            break;
        case CLI:
            ctx->sr &= ~INTERRUPT;
            break;
        case CLV:
            ctx->sr &= ~OVERFLOW;
            break;
        case SEC:
            ctx->sr |= CARRY;
            break;
        case SED:
            ctx->sr |= DECIMAL;
            break;
        case SEI:
            ctx->sr |= INTERRUPT;
            break;

        // system functions

        case BRK:
            // BRK instruction is 2 bytes
            ctx->pc++;
            push_address(ctx, ctx->pc);
            // 6502 quirk, bit 4 and 5 are always set
            push(ctx, ctx->sr | BIT_5 | BIT_4);
            ctx->pc = read_abs_address(ctx->memory, IRQ_ADDRESS);
            ctx->sr |= INTERRUPT;
            break;
        case RTI:
            // ignore bit 4 and 5
            ctx->sr &= (BIT_4 | BIT_5);
            ctx->sr |= pop(ctx) & ~(BIT_4 | BIT_5);
            ctx->pc = pop_address(ctx);
            break;
        case NOP:
            break;

        // unofficial

        case ALR:
            ctx->ac &= read_mem(ctx->memory, address);
            ctx->ac = shift_r(ctx, ctx->ac);
            break;
        case ANC:
            ctx->ac = ctx->ac & read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
            ctx->sr = (ctx->ac & NEGATIVE) ? (CARRY | NEGATIVE): 0;
            ctx->sr |= ((!ctx->ac)? ZERO: 0);
            break;
        case ARR: {
            uint8_t val = ctx->ac & read_mem(ctx->memory, address);
            uint8_t rotated = val >> 1;
            rotated |= (ctx->sr & CARRY) << 7;
            ctx->sr &= ~(CARRY | ZERO | NEGATIVE | OVERFLOW);
            ctx->sr |= (rotated & BIT_6) ? CARRY: 0;
            ctx->sr |= ((rotated & BIT_6) ^ (rotated & BIT_5)) ? OVERFLOW: 0;
            fast_set_ZN(ctx, rotated);
            ctx->ac = rotated;
            break;
        }
        case AXS: {
            uint8_t opr = read_mem(ctx->memory, address);
            ctx->x = ctx->x & ctx->ac;
            uint16_t diff = ctx->x - opr;
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->x = diff;
            fast_set_ZN(ctx, ctx->x);
            break;
        }
        case LAX:
            ctx->ac = read_mem(ctx->memory, address);
            ctx->x = ctx->ac;
            set_ZN(ctx, ctx->ac);
            break;
        case SAX: {
            write_mem(ctx->memory, address, ctx->ac & ctx->x);
            break;
        }
        case DCP: {
            uint8_t m = read_mem(ctx->memory, address) - 1;
            write_mem(ctx->memory, address, m);
            set_ZN(ctx, ctx->ac - m);
            break;
        }
        case ISB: {
            uint8_t m = read_mem(ctx->memory, address) + 1;
            write_mem(ctx->memory, address, m);
            uint16_t diff = ctx->ac - m - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLOW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~m ^ diff) & 0x80) ? OVERFLOW: 0;
            ctx->ac = diff;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case RLA: {
            uint8_t m = rot_l(ctx, read_mem(ctx->memory, address));
            write_mem(ctx->memory, address, m);
            ctx->ac &= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case RRA: {
            uint8_t m = rot_r(ctx, read_mem(ctx->memory, address));
            write_mem(ctx->memory, address, m);
            uint16_t sum = ctx->ac + m + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLOW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY : 0);
            ctx->sr |= ((ctx->ac ^ sum) & (m ^ sum) & 0x80) ? OVERFLOW : 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, sum);
            break;
        }
        case SLO: {
            uint8_t m = shift_l(ctx, read_mem(ctx->memory, address));
            write_mem(ctx->memory, address, m);
            ctx->ac |= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case SRE: {
            uint8_t m = shift_r(ctx, read_mem(ctx->memory, address));
            write_mem(ctx->memory, address, m);
            ctx->ac ^= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        default:
            break;
    }
}

static uint16_t read_abs_address(Memory* mem, uint16_t offset){
    // 16 bit address is little endian so read lo then hi
    uint16_t lo = (uint16_t)read_mem(mem, offset);
    uint16_t hi = (uint16_t)read_mem(mem, offset + 1);
    return (hi << 8) | lo;
}

static uint16_t get_address(c6502* ctx){
    uint16_t addr, hi,lo;
    switch (ctx->instruction->mode) {
        case IMPL:
        case ACC:
        case NONE:
            return 0;
        case REL: {
            int8_t offset = (int8_t)read_mem(ctx->memory, ctx->pc++);
            return ctx->pc + offset;
        }
        case IMT:
            return ctx->pc++;
        case ZPG:
            return read_mem(ctx->memory, ctx->pc++) & 0xFF;
        case ZPG_X:
            addr = read_mem(ctx->memory, ctx->pc++);
            return (addr + ctx->x) & 0xFF;
        case ZPG_Y:
            addr = read_mem(ctx->memory, ctx->pc++);
            return (addr + ctx->y) & 0xFF;
        case ABS:
            addr = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            return addr;
        case ABS_X:
            addr = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            if(has_page_break(addr, addr + ctx->x)) {
                switch (ctx->instruction->opcode) {
                    // these don't take into account absolute x page breaks
                    case STA:case ASL:case DEC:case INC:case LSR:case ROL:case ROR:
                    // unofficial
                    case SLO:case RLA:case SRE:case RRA:case DCP:case ISB:
                        break;
                    default:
                        ctx->cycles++;
                }
            }
            return addr + ctx->x;
        case ABS_Y:
            addr = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            if(has_page_break(addr, addr + ctx->y)) {
                switch (ctx->instruction->opcode) {
                    case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB:
                        break;
                    default:
                        ctx->cycles++;
                }
            }
            return addr + ctx->y;
        case IND:
            addr = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            lo = read_mem(ctx->memory, addr);
            // handle a bug in 6502 hardware where if reading from $xxFF (page boundary) the
            // LSB is read from $xxFF as expected but the MSB is fetched from xx00
            hi = read_mem(ctx->memory, (addr & 0xFF00) | (addr + 1) & 0xFF);
            return (hi << 8) | lo;
        case IDX_IND:
            addr = (read_mem(ctx->memory, ctx->pc++) + ctx->x) & 0xFF;
            hi = read_mem(ctx->memory, (addr + 1) & 0xFF);
            lo = read_mem(ctx->memory, addr & 0xFF);
            return (hi << 8) | lo;
        case IND_IDX:
            addr = read_mem(ctx->memory, ctx->pc++);
            hi = read_mem(ctx->memory, (addr + 1) & 0xFF);
            lo = read_mem(ctx->memory, addr & 0xFF);
            addr = (hi << 8) | lo;
            if(has_page_break(addr, addr + ctx->y)) {
                switch (ctx->instruction->opcode) {
                    case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB:
                        break;
                    default:
                        ctx->cycles++;
                }
            }
            return addr + ctx->y;
    }
    return 0;
}

static void set_ZN(c6502* ctx, uint8_t value){
    ctx->sr &= ~(NEGATIVE | ZERO);
    ctx->sr |= ((!value)? ZERO: 0);
    ctx->sr |= (value & NEGATIVE);
}

static void fast_set_ZN(c6502* ctx, uint8_t value){
    // this assumes the necessary flags (Z & N) have been cleared
    ctx->sr |= ((!value)? ZERO: 0);
    ctx->sr |= (value & NEGATIVE);
}

static void push(c6502* ctx, uint8_t value){
    write_mem(ctx->memory, STACK_START + ctx->sp--, value);
}

static void push_address(c6502* ctx, uint16_t address){
    write_mem(ctx->memory, STACK_START + ctx->sp--, address >> 8);
    write_mem(ctx->memory, STACK_START + ctx->sp--, address & 0xFF);
}

static uint8_t pop(c6502* ctx){
    return read_mem(ctx->memory, STACK_START + ++ctx->sp);
}

static uint16_t pop_address(c6502* ctx){
    uint16_t addr = read_mem(ctx->memory, STACK_START + ++ctx->sp);
    return addr | ((uint16_t)read_mem(ctx->memory, STACK_START + ++ctx->sp)) << 8;
}

static uint8_t shift_l(c6502* ctx, uint8_t val){
    ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
    ctx->sr |= (val & NEGATIVE) ? CARRY: 0;
    val <<= 1;
    fast_set_ZN(ctx, val);
    return val;
}

static uint8_t shift_r(c6502* ctx, uint8_t val){
    ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
    ctx->sr |= (val & 0x1) ? CARRY: 0;
    val >>= 1;
    fast_set_ZN(ctx, val);
    return val;
}

static uint8_t rot_l(c6502* ctx, uint8_t val){
    uint8_t rotated = val << 1;
    rotated |= ctx->sr & CARRY;
    ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
    ctx->sr |= val & NEGATIVE ? CARRY: 0;
    fast_set_ZN(ctx, rotated);
    return rotated;
}

static uint8_t rot_r(c6502* ctx, uint8_t val){
    uint8_t rotated = val >> 1;
    rotated |= (ctx->sr &  CARRY) << 7;
    ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
    ctx->sr |= val & CARRY;
    fast_set_ZN(ctx, rotated);
    return rotated;
}

static void branch(c6502* ctx, uint16_t address, uint8_t mask, uint8_t predicate){
    if(((ctx->sr & mask) > 0) == predicate){
        // increment cycles if branching to a different page
        ctx->cycles += has_page_break(ctx->pc, address);
        ctx->pc = address;
        ctx->cycles++;
    }
}

static uint8_t has_page_break(uint16_t addr1, uint16_t addr2){
    return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}
