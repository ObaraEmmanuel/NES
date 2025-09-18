#include "cpu6502.h"
#include "emulator.h"
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
static void branch(c6502* ctx, uint8_t mask, uint8_t predicate);
static void prep_branch(c6502* ctx);
static uint8_t has_page_break(uint16_t addr1, uint16_t addr2);
static void interrupt_(c6502* ctx);
static void poll_interrupt(c6502* ctx);

void init_cpu(struct Emulator* emulator){
    struct c6502* cpu = &emulator->cpu;
    cpu->emulator = emulator;
    cpu->interrupt = NOI;
    cpu->memory = &emulator->mem;

    cpu->ac = cpu->x = cpu->y = cpu->state = 0;
    cpu->cycles = cpu->dma_cycles = 0;
    cpu->odd_cycle = cpu->t_cycles = 0;
    cpu->sr = 0x24;
    cpu->sp = 0xfd;
#if TRACER == 1 && PROFILE == 0
    cpu->pc = 0xC000;
#else
    cpu->pc = read_abs_address(cpu->memory, RESET_ADDRESS);
#endif
}

void reset_cpu(c6502* cpu){
    cpu->sr |= INTERRUPT;
    cpu->sp -= 3;
    cpu->pc = read_abs_address(cpu->memory, RESET_ADDRESS);
    cpu->cycles = 0;
    cpu->dma_cycles = 0;
}

static void interrupt_(c6502* ctx){
    // handle interrupt
    uint16_t addr;
    uint8_t set_brk = 0;

    if(ctx->interrupt & BRK_I) {
        // BRK instruction is 2 bytes
        ctx->pc++;
        if(ctx->state & NMI_HIJACK) {
            addr = NMI_ADDRESS;
            ctx->interrupt &= ~NMI;
        }else {
            addr = IRQ_ADDRESS;
        }
        ctx->interrupt &= ~BRK_I;
        // re-apply Break flag
        set_brk = 1;
    }
    else if(ctx->interrupt & NMI) {
        addr = NMI_ADDRESS;
        // NMI is edge triggered so clear it
        ctx->interrupt &= ~NMI;
    }
    else if(ctx->interrupt & RSI){
        addr = RESET_ADDRESS;
        // not used but just in case
        interrupt_clear(ctx, RSI);
    }
    else if(ctx->interrupt & IRQ) {
        addr = IRQ_ADDRESS;
        // ctx->sr |= BREAK;
    } else {
        LOG(ERROR, "No interrupt set");
        return;
    }

    push_address(ctx, ctx->pc);
    // bit 5 always set, bit 4 set on BRK
    push(ctx, ctx->sr & ~BIT_4 | BIT_5 | (set_brk ? BIT_4 : 0));
    ctx->sr |= INTERRUPT;
    ctx->pc = read_abs_address(ctx->memory, addr);
}

void interrupt(c6502* ctx, const Interrupt code) {
    if(code == NMI) {
        if(!ctx->NMI_line) {
            ctx->interrupt |= NMI;
        }
        ctx->NMI_line = 1;
    }else {
        ctx->interrupt |= code;
    }
}
void interrupt_clear(c6502* ctx, const Interrupt code) {
    if(code == NMI) ctx->NMI_line = 0;
    else ctx->interrupt &= ~code;
}

void do_DMA(c6502* ctx, size_t cycles) {
    ctx->dma_cycles += cycles;
    ctx->state |= DMA_OCCURRED;
}

static void branch(c6502* ctx, uint8_t mask, uint8_t predicate) {
    if(((ctx->sr & mask) > 0) == predicate){
        // increment cycles if branching to a different page
        ctx->cycles += has_page_break(ctx->pc, ctx->address);
        ctx->cycles++;
        // tell the cpu to perform a branch when it is ready
        ctx->state |= BRANCH_STATE;
    }else {
        // remove branch state
        ctx->state &= ~BRANCH_STATE;
    }
}

static void prep_branch(c6502* ctx){
    switch(ctx->instruction->opcode){
        case BCC:
            branch(ctx, CARRY, 0);
            break;
        case BCS:
            branch(ctx, CARRY, 1);
            break;
        case BEQ:
            branch(ctx, ZERO, 1);
            break;
        case BMI:
            branch(ctx, NEGATIVE, 1);
            break;
        case BNE:
            branch(ctx, ZERO, 0);
            break;
        case BPL:
            branch(ctx, NEGATIVE, 0);
            break;
        case BVC:
            branch(ctx, OVERFLW, 0);
            break;
        case BVS:
            branch(ctx, OVERFLW, 1);
            break;
        default:
            ctx->state &= ~BRANCH_STATE;
    }
}

static void poll_interrupt(c6502* ctx) {
    ctx->state |= INTERRUPT_POLLED;
    if(ctx->interrupt & ~IRQ || (ctx->interrupt & IRQ && !(ctx->sr & INTERRUPT))){
        // prepare for interrupts
        ctx->state |= INTERRUPT_PENDING;
        // check if NMI is asserted at poll time
        ctx->state |= ctx->interrupt & NMI ? NMI_ASSERTED : 0;
    }
}


void execute(c6502* ctx){
    ctx->odd_cycle ^= 1;
    ctx->t_cycles++;
    if (ctx->dma_cycles != 0){
        // DMA CPU suspend
        ctx->dma_cycles--;
        return;
    }
    if(ctx->cycles == 0) {
#if TRACER == 1
        print_cpu_trace(ctx);
#endif
        if(!(ctx->state & INTERRUPT_POLLED)) {
            poll_interrupt(ctx);
        }
        ctx->state &= ~(INTERRUPT_POLLED | NMI_HIJACK | DMA_OCCURRED);
        if(ctx->state & INTERRUPT_PENDING) {
            // takes 7 cycles and this is one of them so only 6 are left
            ctx->cycles = 6;
            return;
        }

        uint8_t opcode = read_mem(ctx->memory, ctx->pc++);
        ctx->instruction = &instructionLookup[opcode];
        ctx->address = get_address(ctx);
        if(ctx->instruction->opcode == BRK) {
            // set BRK interrupt
            ctx->interrupt |= BRK_I;
            ctx->state |= INTERRUPT_PENDING;
            ctx->cycles = 6;
            return;
        }
        ctx->cycles += cycleLookup[opcode];
        // prepare for branching and adjust cycles accordingly
        prep_branch(ctx);
        ctx->cycles--;
        return;
    }

    if(ctx->cycles == 1){
        ctx->cycles--;
        // proceed to execution
    } else{
        // delay execution
        if(ctx->state & INTERRUPT_PENDING && ctx->cycles == 4) {
            if(ctx->interrupt & NMI) ctx->state |= NMI_HIJACK;
        }
        ctx->cycles--;
        return;
    }

    // handle interrupt
    if(ctx->state & INTERRUPT_PENDING) {
        interrupt_(ctx);
        ctx->state &= ~INTERRUPT_PENDING;
        return;
    }

    uint16_t address = ctx->address;

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
            // poll before updating I flag
            poll_interrupt(ctx);
            ctx->sr = pop(ctx);
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
            ctx->sr &= ~(NEGATIVE | OVERFLW | ZERO);
            ctx->sr |= (!(opr & ctx->ac) ? ZERO: 0);
            ctx->sr |= (opr & (NEGATIVE | OVERFLW));
            break;
        }

        // Arithmetic opcodes

        case ADC: {
            uint8_t opr = read_mem(ctx->memory, address);
            uint16_t sum = ctx->ac + opr + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY: 0);
            ctx->sr |= ((ctx->ac ^ sum) & (opr ^ sum) & 0x80) ? OVERFLW: 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case SBC: {
            uint8_t opr = read_mem(ctx->memory, address);
            uint16_t diff = ctx->ac - opr - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~opr ^ diff) & 0x80) ? OVERFLW: 0;
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
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m--);
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
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m++);
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
                uint8_t m = read_mem(ctx->memory, address);
                // dummy write
                write_mem(ctx->memory, address, m);
                write_mem(ctx->memory, address, shift_l(ctx, m));
            }
            break;
        case LSR:
            if(ctx->instruction->mode == ACC) {
                ctx->ac = shift_r(ctx, ctx->ac);
            }else{
                uint8_t m = read_mem(ctx->memory, address);
                // dummy write
                write_mem(ctx->memory, address, m);
                write_mem(ctx->memory, address, shift_r(ctx, m));
            }
            break;
        case ROL:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_l(ctx, ctx->ac);
            }else{
                uint8_t m = read_mem(ctx->memory, address);
                // dummy write
                write_mem(ctx->memory, address, m);
                write_mem(ctx->memory, address, rot_l(ctx, m));
            }
            break;
        case ROR:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_r(ctx, ctx->ac);
            }else{
                uint8_t m = read_mem(ctx->memory, address);
                // dummy write
                write_mem(ctx->memory, address, m);
                write_mem(ctx->memory, address, rot_r(ctx, m));
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

        case BCC:case BCS:case BEQ:case BMI:case BNE:case BPL:case BVC:case BVS:
            if(ctx->state & BRANCH_STATE) {
                ctx->pc = ctx->address;
                ctx->state &= ~BRANCH_STATE;
            }
            break;

        // status flag changes

        case CLC:
            ctx->sr &= ~CARRY;
            break;
        case CLD:
            ctx->sr &= ~DECIMAL_;
            break;
        case CLI:
            // poll before updating I flag
            poll_interrupt(ctx);
            ctx->sr &= ~INTERRUPT;
            break;
        case CLV:
            ctx->sr &= ~OVERFLW;
            break;
        case SEC:
            ctx->sr |= CARRY;
            break;
        case SED:
            ctx->sr |= DECIMAL_;
            break;
        case SEI:
            poll_interrupt(ctx);
            ctx->sr |= INTERRUPT;
            break;

        // system functions

        case BRK:
            // Handled in interrupt_()
            break;
        case RTI:
            // ignore bit 4 and 5
            ctx->sr &= (BIT_4 | BIT_5);
            ctx->sr |= pop(ctx) & ~(BIT_4 | BIT_5);
            ctx->pc = pop_address(ctx);
            break;
        case NOP:
            if(ctx->instruction->mode == ABS) {
                // dummy read edge case for unofficial opcode Ox0C
                read_mem(ctx->memory, address);
            }
            break;

        // unofficial

        case ALR:
            // Unstable instruction.
            // A <- A & (const | M), A <- LSR A
            // magic const set to 0x00 but, could also be 0xff, 0xee, ..., 0x11 depending on analog effects
            ctx->ac &= read_mem(ctx->memory, address);
            ctx->ac = shift_r(ctx, ctx->ac);
            break;
        case ANC:
            ctx->ac = ctx->ac & read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
            ctx->sr |= (ctx->ac & NEGATIVE) ? (CARRY | NEGATIVE): 0;
            ctx->sr |= ((!ctx->ac)? ZERO: 0);
            break;
        case ANE:
            // Unstable instruction.
            // A <- (A | const) & X & M
            // magic const set to 0xff but, could also be 0xee, 0xdd, ..., 0x00 depending on analog effects
            ctx->ac = ctx->x & read_mem(ctx->memory, address);
            set_ZN(ctx, ctx->ac);
            break;
        case ARR: {
            // Unstable instruction.
            // A <- A & (const | M), ROR A
            // magic const set to 0x00 but, could also be 0xff, 0xee, ..., 0x11 depending on analog effects
            uint8_t val = ctx->ac & read_mem(ctx->memory, address);
            uint8_t rotated = val >> 1;
            rotated |= (ctx->sr & CARRY) << 7;
            ctx->sr &= ~(CARRY | ZERO | NEGATIVE | OVERFLW);
            ctx->sr |= (rotated & BIT_6) ? CARRY: 0;
            ctx->sr |= (((rotated & BIT_6) >> 1) ^ (rotated & BIT_5)) ? OVERFLW: 0;
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
            // Unstable instruction.
            // A,X <- (A | const) & M
            // magic const set to 0xff but, could also be 0xee, 0xdd, ..., 0x00 depending on analog effects
            ctx->ac = read_mem(ctx->memory, address);
            ctx->x = ctx->ac;
            set_ZN(ctx, ctx->ac);
            break;
        case SAX: {
            write_mem(ctx->memory, address, ctx->ac & ctx->x);
            break;
        }
        case DCP: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m--);
            write_mem(ctx->memory, address, m);
            uint16_t diff = ctx->ac - read_mem(ctx->memory, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case ISB: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m++);
            write_mem(ctx->memory, address, m);
            uint16_t diff = ctx->ac - m - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~m ^ diff) & 0x80) ? OVERFLW: 0;
            ctx->ac = diff;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case RLA: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m);
            m = rot_l(ctx, m);
            write_mem(ctx->memory, address, m);
            ctx->ac &= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case RRA: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m);
            m = rot_r(ctx, m);
            write_mem(ctx->memory, address, m);
            uint16_t sum = ctx->ac + m + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY : 0);
            ctx->sr |= ((ctx->ac ^ sum) & (m ^ sum) & 0x80) ? OVERFLW : 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, sum);
            break;
        }
        case SLO: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m);
            m = shift_l(ctx, m);
            write_mem(ctx->memory, address, m);
            ctx->ac |= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case SRE: {
            uint8_t m = read_mem(ctx->memory, address);
            // dummy write
            write_mem(ctx->memory, address, m);
            m = shift_r(ctx, m);
            write_mem(ctx->memory, address, m);
            ctx->ac ^= m;
            set_ZN(ctx, ctx->ac);
            break;
        }

        // The next 4 instructions are extremely unstable with weird sometimes inexplicable behavior
        // I have tried to emulate them to the best of my ability
        case SHY: {
            // if RDY line goes low 2 cycles before the value write, the high byte of target address is ignored
            // By target address I mean the unindexed address as read from memory referred to here as raw_address
            // I have equated this behaviour with DMA occurring in the course of the instruction
            // This also applies to SHX, SHA and SHS
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->y: ctx->y & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            write_mem(ctx->memory, address, val);
            break;
        }
        case SHX: {
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x: ctx->x & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            write_mem(ctx->memory, address, val);
            break;
        }
        case SHA: {
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x & ctx->ac: ctx->x & ctx->ac & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            write_mem(ctx->memory, address, val);
            break;
        }
        case SHS:
            ctx->sp = ctx->ac & ctx->x;
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x & ctx->ac: ctx->x & ctx->ac & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            write_mem(ctx->memory, address, val);
            break;
        case LAS:
            ctx->ac = ctx->x = ctx->sp = read_mem(ctx->memory, address) & ctx->sp;;
            set_ZN(ctx, ctx->ac);
            break;
        default:
            break;
    }
}

static uint16_t read_abs_address(Memory* mem, uint16_t offset){
    // 16-bit address is little endian so read lo then hi
    uint16_t lo = (uint16_t)read_mem(mem, offset);
    uint16_t hi = (uint16_t)read_mem(mem, offset + 1);
    return (hi << 8) | lo;
}

static uint16_t get_address(c6502* ctx){
    uint16_t addr, hi,lo;
    switch (ctx->instruction->mode) {
        case IMPL:
        case ACC:
            // dummy read
            read_mem(ctx->memory, ctx->pc);
        case NONE:
            ctx->raw_address = 0;
            return 0;
        case REL: {
            int8_t offset = (int8_t)read_mem(ctx->memory, ctx->pc++);
            addr = ctx->raw_address = ctx->pc + offset;
            return addr;
        }
        case IMT:
            ctx->raw_address = ctx->pc + 1;
            return ctx->pc++;
        case ZPG:
            ctx->raw_address = ctx->pc + 1;
            return read_mem(ctx->memory, ctx->pc++);
        case ZPG_X:
            addr = ctx->raw_address = ctx->raw_address = read_mem(ctx->memory, ctx->pc++);
            return (addr + ctx->x) & 0xFF;
        case ZPG_Y:
            addr = ctx->raw_address = read_mem(ctx->memory, ctx->pc++);
            return (addr + ctx->y) & 0xFF;
        case ABS:
            addr = ctx->raw_address = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            return addr;
        case ABS_X:
            addr = ctx->raw_address = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            switch (ctx->instruction->opcode) {
                // these don't take into account absolute x page breaks
                case STA:case ASL:case DEC:case INC:case LSR:case ROL:case ROR:
                // unofficial
                case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case SHY:
                    // invalid read
                    read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->x) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->x)) {
                        // invalid read
                        read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->x) & 0xff));
                        ctx->cycles++;
                    }
            }
            return addr + ctx->x;
        case ABS_Y:
            addr = ctx->raw_address = read_abs_address(ctx->memory, ctx->pc);
            ctx->pc += 2;
            switch (ctx->instruction->opcode) {
                case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case NOP:
                case SHX:case SHA:case SHS:
                    // invalid read
                    read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->y)) {
                        // invalid read
                        read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                        ctx->cycles++;
                    }
            }
            return addr + ctx->y;
        case IND:
            addr = ctx->raw_address = read_abs_address(ctx->memory, ctx->pc);
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
            addr = ctx->raw_address = (hi << 8) | lo;
            return addr;
        case IND_IDX:
            addr = read_mem(ctx->memory, ctx->pc++);
            hi = read_mem(ctx->memory, (addr + 1) & 0xFF);
            lo = read_mem(ctx->memory, addr & 0xFF);
            addr = ctx->raw_address = (hi << 8) | lo;
            switch (ctx->instruction->opcode) {
                case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case NOP:
                case SHA:
                    // invalid read
                    read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->y)) {
                        // invalid read
                        read_mem(ctx->memory, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
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

static uint8_t has_page_break(uint16_t addr1, uint16_t addr2){
    return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}
