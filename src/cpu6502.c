#include "cpu6502.h"
#include "emulator.h"
#include "utils.h"


static uint8_t read(c6502* ctx, uint16_t address);
static void write(c6502* ctx, uint16_t address, uint8_t value);
static void tick_dma(c6502* ctx, DMA* dma);
static uint16_t get_address(c6502* ctx);
static uint16_t read_abs_address(c6502 *ctx, uint16_t offset);
static uint16_t read_abs_address_(c6502 *ctx, uint16_t offset);
static void set_ZN(c6502* ctx, uint8_t value);
static void fast_set_ZN(c6502* ctx, uint8_t value);
static uint8_t shift_l(c6502* ctx, uint8_t val);
static uint8_t shift_r(c6502* ctx, uint8_t val);
static uint8_t rot_l(c6502* ctx, uint8_t val);
static uint8_t rot_r(c6502* ctx, uint8_t val);
static void push(c6502* ctx, uint8_t value);
static void push_address(c6502* ctx, uint16_t address);
static void push_address_(c6502* ctx, uint16_t address);
static uint8_t pop(c6502* ctx);
static uint8_t pop_(c6502* ctx);
static uint16_t pop_address(c6502* ctx);
static uint16_t pop_address_(c6502* ctx);
static uint8_t has_page_break(uint16_t addr1, uint16_t addr2);
static void interrupt_(c6502* ctx);
static void poll_interrupt(c6502* ctx);
static void rti(c6502* ctx);
static void rti_(c6502* ctx);

void init_cpu(struct Emulator* emulator){
    struct c6502* cpu = &emulator->cpu;
    cpu->emulator = emulator;
    cpu->interrupt = NOI;
    cpu->polled_interrupt = NOI;
    set_cpu_mode(cpu, CPU_EXEC); // Normal execution
    cpu->memory = &emulator->mem;
    cpu->ppu = &emulator->ppu;
    cpu->apu = &emulator->apu;
    cpu->sr_started = 0;
    cpu->NMI_hook = NULL;

    cpu->oam.type = DMA_OAM;
    cpu->dmc.type = DMA_DMC;

    cpu->ac = cpu->x = cpu->y = cpu->state = 0;
    cpu->t_cycles = 0;
    cpu->sr = 0x24;
    cpu->sp = 0xfd;
#if TRACER == 1 && PROFILE == 0
    cpu->pc = 0xC000;
#else
    cpu->pc = read_abs_address_(cpu, RESET_ADDRESS);
#endif
}

void reset_cpu(c6502* cpu){
    cpu->sr |= INTERRUPT;
    set_cpu_mode(cpu, CPU_EXEC); // Normal execution
    cpu->sp -= 3;
    cpu->pc = read_abs_address_(cpu, RESET_ADDRESS);
    cpu->t_cycles = 0;
    cpu->sr_started = 0;
    cpu->NMI_hook = NULL;
    cpu->dmc.phase = DMA_CLEAR;
    cpu->oam.phase = DMA_CLEAR;
}

void schedule_dma(c6502* ctx, DMA_Type type, uint8_t delay, uint16_t src, uint8_t* dst, uint16_t len) {
    DMA* dma = type == DMA_DMC? &ctx->dmc : &ctx->oam;
    if (!(dma->phase == DMA_CLEAR)) {
        return;
    }
    dma->schedule = delay;
    dma->src_address = src;
    dma->dst = dst;
    dma->length = len;
    dma->index = 0;
    if (!delay)
        dma->phase = DMA_HALTING;
    else
        dma->phase = DMA_SCHEDULED;
}

static void tick_dma(c6502* ctx, DMA* dma) {
    switch (dma->phase) {
        case DMA_SCHEDULED:
            if (dma->schedule) {
                dma->schedule--;
                if (!dma->schedule)
                    dma->phase = DMA_HALTING;
            } else
                dma->phase = DMA_HALTING;
            break;
        case DMA_ALIGNING:
            dma->phase = DMA_READ;
            break;
        case DMA_HALTING:
            if (dma->type == DMA_DMC)
                dma->phase = DMA_DUMMY;
            else if (ctx->apu->cycles & 1)
                dma->phase = DMA_ALIGNING;
            else
                dma->phase = DMA_READ;
            break;
        case DMA_DUMMY:
            if (ctx->apu->cycles & 1)
                dma->phase = DMA_ALIGNING;
            else
                dma->phase = DMA_READ;
            break;
        case DMA_READ:
            dma->buffer = read_mem(ctx->memory, dma->src_address + dma->index);
            if (dma->type == DMA_OAM)
                dma->phase = DMA_WRITE;
            else {
                // DMC DMA copies just 1 byte and has no write cycle
                dma->dst[dma->index++] = dma->buffer;
                dma->phase = DMA_CLEAR;
                // inform APU byte is ready
                dmc_complete(ctx->apu);
            }
            break;
        case DMA_WRITE:
            dma->dst[dma->index++] = dma->buffer;
            if (dma->index >= dma->length) {
                dma->phase = DMA_CLEAR;
            } else
                dma->phase = DMA_READ;
            break;
        case DMA_CLEAR:
        default:
            break;
    }
}

static uint8_t read(c6502* ctx, uint16_t address) {
    uint8_t was_active = 0;
    while (ctx->dmc.phase & DMA_ACTIVE || ctx->oam.phase & DMA_ACTIVE) {
        if (ctx->dmc.phase < DMA_READ && ctx->oam.phase < DMA_READ) {
            // dummy read
            read_mem(ctx->memory, address);
        }
        if (ctx->dmc.phase == DMA_READ && ctx->oam.phase == DMA_READ) {
            ctx->oam.phase = DMA_ALIGNING;
            tick_dma(ctx, &ctx->dmc);
        } else {
            tick_dma(ctx, &ctx->dmc);
            tick_dma(ctx, &ctx->oam);
        }
        tick_master_clock(ctx->emulator);
        was_active = 1;
    }
    if (!was_active) {
        if (ctx->oam.phase == DMA_SCHEDULED)
            tick_dma(ctx, &ctx->oam);
        if (ctx->dmc.phase == DMA_SCHEDULED)
            tick_dma(ctx, &ctx->dmc);
    } else
        ctx->state |= DMA_OCCURRED;

    uint8_t val = read_mem(ctx->memory, address);
    tick_master_clock(ctx->emulator);
    return val;
}
static void write(c6502* ctx, uint16_t address, uint8_t value) {
    if (ctx->oam.phase == DMA_SCHEDULED)
        tick_dma(ctx, &ctx->oam);
    if (ctx->dmc.phase == DMA_SCHEDULED)
        tick_dma(ctx, &ctx->dmc);

    write_mem(ctx->memory, address, value);
    tick_master_clock(ctx->emulator);
}

uint8_t run_cpu_subroutine(c6502* ctx, uint16_t address) {
    if (ctx->mode & CPU_SR_ANY) {
        // unable to begin subroutine because there is an executing subroutine and/or ISR
        LOG(TRACE, "Unable to start subroutine $%4x, pending subroutine %x",address, ctx->sub_address);
        return CPU_SR;
    }
    ctx->sub_address = address;
    set_cpu_mode(ctx, ctx->mode | CPU_SR);
    LOG(TRACE, "Running subroutine: $%4x",ctx->sub_address);
    return 0;
}

void set_cpu_mode(c6502* ctx, CPUMode mode) {
    ctx->mode = mode;
    LOG(TRACE, "CPU mode: %d",mode);
}

static void interrupt_(c6502* ctx){
    // handle interrupt
    uint16_t addr;
    uint8_t set_brk = 0;
    uint8_t interrupt = ctx->polled_interrupt;
    ctx->polled_interrupt = 0;
    push_address(ctx, ctx->pc);

    if(interrupt & BRK_I) {
        ctx->interrupt &= ~BRK_I;
        // re-apply Break flag
        set_brk = 1;
        addr = IRQ_ADDRESS;
        // NMI hijack
        if (ctx->interrupt & NMI) {
            addr = NMI_ADDRESS;
            ctx->interrupt &= ~NMI;
        }
    }
    else if(interrupt & NMI) {
        addr = NMI_ADDRESS;
        // NMI is edge triggered so clear it
        ctx->interrupt &= ~NMI;
    }
    else if(interrupt & IRQ) {
        addr = IRQ_ADDRESS;
        // NMI hijack
        if (ctx->interrupt & NMI) {
            addr = NMI_ADDRESS;
            ctx->interrupt &= ~NMI;
        }
    } else {
        LOG(ERROR, "No interrupt set");
        return;
    }

    // bit 5 always set, bit 4 set on BRK
    push(ctx, ctx->sr & ~BIT_4 | BIT_5 | (set_brk ? BIT_4 : 0));
    ctx->sr |= INTERRUPT;
    ctx->pc = read_abs_address(ctx, addr);
    if (addr == NMI_ADDRESS && ctx->NMI_hook != NULL) {
        // call NMI hook for phase 0 (pre ISR)
        push_address_(ctx, ctx->sub_address);
        ctx->NMI_hook(ctx, 0);
        ctx->sr_started = 0;
        LOG(TRACE, "NMI wrapper at $%4x", ctx->sub_address);
        set_cpu_mode(ctx, ctx->mode | CPU_NMI_SR);
    }
}

static void rti(c6502* ctx) {
    // dummy read stack
    read(ctx, STACK_START + ctx->sp);
    // ignore bit 4 and 5
    ctx->sr &= (BIT_4 | BIT_5);
    ctx->sr |= pop(ctx) & ~(BIT_4 | BIT_5);
    ctx->pc = pop_address(ctx);
    // hopefully intrinsic 6502 x-tics will allow this to be enough
    // i.e. no ISR nesting
    if (ctx->mode & CPU_ISR) {
        LOG(TRACE, "ISR completed");
        set_cpu_mode(ctx, ctx->mode & ~CPU_ISR);
    }
}

static void rti_(c6502* ctx) {
    // rti with no side effects
    // ignore bit 4 and 5
    ctx->sr &= (BIT_4 | BIT_5);
    ctx->sr |= pop_(ctx) & ~(BIT_4 | BIT_5);
    ctx->pc = pop_address_(ctx);
    // hopefully intrinsic 6502 x-tics will allow this to be enough
    // i.e. no ISR nesting
    if (ctx->mode & CPU_ISR) {
        LOG(TRACE, "ISR completed");
        set_cpu_mode(ctx, ctx->mode & ~CPU_ISR);
    }
}

void interrupt(c6502* ctx, const Interrupt code) {
    if(code == NMI) {
        if(!ctx->NMI_line) {
            // NMI line rising edge detected
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

static uint8_t is_branch_taken(c6502* ctx){
    switch(ctx->instruction->opcode){
        case BCC:
            return (ctx->sr & CARRY) == 0;
        case BCS:
            return (ctx->sr & CARRY) > 0;
        case BEQ:
            return (ctx->sr & ZERO) > 0;
        case BNE:
            return (ctx->sr & ZERO) == 0;
        case BMI:
            return (ctx->sr & NEGATIVE) > 0;
        case BPL:
            return (ctx->sr & NEGATIVE) == 0;
        case BVC:
            return (ctx->sr & OVERFLW) == 0;
        case BVS:
            return (ctx->sr & OVERFLW) > 0;
        default:
            return 0;
    }
}

static void poll_interrupt(c6502* ctx) {
    if(ctx->interrupt & ~IRQ || (ctx->interrupt & IRQ && !(ctx->sr & INTERRUPT))){
        // prepare for interrupts
        ctx->polled_interrupt = ctx->interrupt;
    } else {
        ctx->polled_interrupt = 0;
    }
}


void execute(c6502* ctx){

    if (!ctx->mode) {
        // cpu wait mode
        read(ctx, 0x0000);
        return;
    }
    // if there is a pending ISR, we wait before we start the subroutine unless it is NMI
    if ((ctx->mode & CPU_NMI_SR || (ctx->mode & (CPU_SR | CPU_ISR)) == CPU_SR) && ctx->sr_started == 0) {
        // we just started cpu subroutine mode
        // let's do a manual JSR to sub_address
        push_address_(ctx, ctx->pc);
        push_address_(ctx, SENTINEL_ADDR);
        ctx->pc = ctx->sub_address;
        ctx->sr_started = 1;
    }

#if TRACER == 1
    print_cpu_trace(ctx);
#endif
    if(ctx->polled_interrupt) {
        // There is a pending interrupt
        LOG(TRACE, "Starting ISR: $%2x", ctx->polled_interrupt);
        // read opcode without incrementing PC
        read(ctx, ctx->pc);
        // dummy read next byte
        read(ctx, ctx->pc + 1);
        interrupt_(ctx);
        set_cpu_mode(ctx, ctx->mode | CPU_ISR);
        return;
    }
    if (!(ctx->mode & CPU_EXEC_ANY)) {
        // busy wait if not in any execution mode but listen for interrupts
        poll_interrupt(ctx);
        read(ctx, 0x0000);
        return;
    }
    ctx->state &= ~DMA_OCCURRED;
    uint8_t opcode = read(ctx, ctx->pc++);
    ctx->instruction = &instructionLookup[opcode];
    ctx->address = get_address(ctx);

    uint16_t address = ctx->address;

    switch (ctx->instruction->opcode) {

        // Load and store opcodes

        case LDA:
            poll_interrupt(ctx);
            ctx->ac = read(ctx, address);
            set_ZN(ctx, ctx->ac);
            break;
        case LDX:
            poll_interrupt(ctx);
            ctx->x = read(ctx, address);
            set_ZN(ctx, ctx->x);
            break;
        case LDY:
            poll_interrupt(ctx);
            ctx->y = read(ctx, address);
            set_ZN(ctx, ctx->y);
            break;
        case STA:
            poll_interrupt(ctx);
            write(ctx, address, ctx->ac);
            break;
        case STY:
            poll_interrupt(ctx);
            write(ctx, address, ctx->y);
            break;
        case STX:
            poll_interrupt(ctx);
            write(ctx, address, ctx->x);
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
            // dummy read
            read(ctx, ctx->pc);
            poll_interrupt(ctx);
            push(ctx, ctx->ac);
            break;
        case PHP:
            // dummy read
            read(ctx, ctx->pc);
            poll_interrupt(ctx);
            // 6502 quirk bit 4 and 5 are always set by this instruction: see also BRK
            push(ctx, ctx->sr | BIT_4 | BIT_5);
            break;
        case PLA:
            // dummy read
            read(ctx, ctx->pc);
            // dummy read from stack
            read(ctx, STACK_START + ctx->sp);
            poll_interrupt(ctx);
            ctx->ac = pop(ctx);
            set_ZN(ctx, ctx->ac);
            break;
        case PLP:
            // dummy read
            read(ctx, ctx->pc);
            // dummy read from stack
            read(ctx, STACK_START + ctx->sp);
            poll_interrupt(ctx);
            ctx->sr = pop(ctx);
            break;

        // logical opcodes

        case AND:
            poll_interrupt(ctx);
            ctx->ac &= read(ctx, address);
            set_ZN(ctx, ctx->ac);
            break;
        case EOR:
            poll_interrupt(ctx);
            ctx->ac ^= read(ctx, address);
            set_ZN(ctx, ctx->ac);
            break;
        case ORA:
            poll_interrupt(ctx);
            ctx->ac |= read(ctx, address);
            set_ZN(ctx, ctx->ac);
            break;
        case BIT: {
            poll_interrupt(ctx);
            uint8_t opr = read(ctx, address);
            ctx->sr &= ~(NEGATIVE | OVERFLW | ZERO);
            ctx->sr |= (!(opr & ctx->ac) ? ZERO: 0);
            ctx->sr |= (opr & (NEGATIVE | OVERFLW));
            break;
        }

        // Arithmetic opcodes

        case ADC: {
            poll_interrupt(ctx);
            uint8_t opr = read(ctx, address);
            uint16_t sum = ctx->ac + opr + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY: 0);
            ctx->sr |= ((ctx->ac ^ sum) & (opr ^ sum) & 0x80) ? OVERFLW: 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case SBC: {
            poll_interrupt(ctx);
            uint8_t opr = read(ctx, address);
            uint16_t diff = ctx->ac - opr - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~opr ^ diff) & 0x80) ? OVERFLW: 0;
            ctx->ac = diff;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case CMP: {
            poll_interrupt(ctx);
            uint8_t opr = read(ctx, address);
            uint16_t diff = ctx->ac - opr;
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case CPX: {
            poll_interrupt(ctx);
            uint16_t diff = ctx->x - read(ctx, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0x100) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case CPY: {
            poll_interrupt(ctx);
            uint16_t diff = ctx->y - read(ctx, address);
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }

        // Increments and decrements opcodes

        case DEC:{
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m--);
            poll_interrupt(ctx);
            write(ctx, address, m);
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
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m++);
            poll_interrupt(ctx);
            write(ctx, address, m);
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
                uint8_t m = read(ctx, address);
                // dummy write
                write(ctx, address, m);
                poll_interrupt(ctx);
                write(ctx, address, shift_l(ctx, m));
            }
            break;
        case LSR:
            if(ctx->instruction->mode == ACC) {
                ctx->ac = shift_r(ctx, ctx->ac);
            }else{
                uint8_t m = read(ctx, address);
                // dummy write
                write(ctx, address, m);
                poll_interrupt(ctx);
                write(ctx, address, shift_r(ctx, m));
            }
            break;
        case ROL:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_l(ctx, ctx->ac);
            }else{
                uint8_t m = read(ctx, address);
                // dummy write
                write(ctx, address, m);
                poll_interrupt(ctx);
                write(ctx, address, rot_l(ctx, m));
            }
            break;
        case ROR:
            if(ctx->instruction->mode == ACC){
                ctx->ac = rot_r(ctx, ctx->ac);
            }else{
                uint8_t m = read(ctx, address);
                // dummy write
                write(ctx, address, m);
                poll_interrupt(ctx);
                write(ctx, address, rot_r(ctx, m));
            }
            break;

        // jumps and calls

        case JMP:
            ctx->pc = address;
            break;
        case JSR: {
            uint8_t lo = read(ctx, ctx->pc++);
            // dummy stack read
            read(ctx, STACK_START + ctx->sp);
            push_address(ctx, ctx->pc);
            poll_interrupt(ctx);
            // read high byte of addr and set pc
            ctx->pc = lo | read(ctx, ctx->pc) << 8;
            break;
        }
        case RTS:
            // dummy read
            read(ctx, ctx->pc);
            // dummy stack read
            read(ctx, STACK_START + ctx->sp);
            ctx->pc = pop_address(ctx);
            poll_interrupt(ctx);
            // dummy read
            read(ctx, ctx->pc++);
            // Keep track of external subroutine
            if ((ctx->mode & CPU_SR_ANY) && ctx->pc == SENTINEL_ADDR + 1) {
                ctx->pc = pop_address_(ctx);
                ctx->memory->bus = ctx->pc >> 8;
                if (ctx->mode & CPU_NMI_SR) {
                    if (ctx->NMI_hook != NULL) ctx->NMI_hook(ctx, 1);
                    LOG(TRACE, "NMI wrapper at $%4x ended", ctx->sub_address);
                    ctx->sub_address = pop_address_(ctx);
                    set_cpu_mode(ctx, ctx->mode & ~CPU_NMI_SR);
                    if (!(ctx->mode & CPU_SR)) ctx->sr_started = 0;
                    // end NMI
                    rti_(ctx);
                }else {
                    LOG(TRACE, "Exiting subroutine $%4x", ctx->sub_address);
                    set_cpu_mode(ctx, ctx->mode & ~CPU_SR);
                    ctx->sr_started = 0;
                }
            }
            break;

        // branching opcodes

        case BCC:case BCS:case BEQ:case BMI:case BNE:case BPL:case BVC:case BVS: {
            poll_interrupt(ctx);
            int8_t offset = (int8_t)read(ctx, ctx->pc++);
            if(is_branch_taken(ctx)) {
                // dummy read
                read(ctx, ctx->pc);
                if (has_page_break(ctx->pc + offset, ctx->pc)) {
                    uint8_t first_poll = ctx->polled_interrupt;
                    poll_interrupt(ctx);
                    // make sure old interrupts are still retained
                    ctx->polled_interrupt |= first_poll;
                    // dummy read at invalid address
                    read(ctx, ctx->pc & 0xff00 | (ctx->pc & 0xff) + offset & 0xff);
                }
                // set PC to the correct offset
                ctx->pc += offset;
            }
            break;
        }

        // status flag changes

        case CLC:
            ctx->sr &= ~CARRY;
            break;
        case CLD:
            ctx->sr &= ~DECIMAL_;
            break;
        case CLI:
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
            ctx->sr |= INTERRUPT;
            break;

        // system functions

        case BRK:
            // dummy read next instruction
            read(ctx, ctx->pc++);
            // set BRK interrupt
            ctx->polled_interrupt |= BRK_I;
            interrupt_(ctx);
            break;
        case RTI:
            // dummy read next byte
            read(ctx, ctx->pc);
            rti(ctx);
            poll_interrupt(ctx);
            break;
        case NOP:
            if (ctx->instruction->mode != ACC && ctx->instruction->mode != NONE && ctx->instruction->mode != IMPL) {
                poll_interrupt(ctx);
                // dummy read
                read(ctx, address);
            }
            break;

        // unofficial

        case ALR:
            // Unstable instruction.
            // A <- A & (const | M), A <- LSR A
            // magic const set to 0x00 but, could also be 0xff, 0xee, ..., 0x11 depending on analog effects
            poll_interrupt(ctx);
            ctx->ac &= read(ctx, address);
            ctx->ac = shift_r(ctx, ctx->ac);
            break;
        case ANC:
            poll_interrupt(ctx);
            ctx->ac = ctx->ac & read(ctx, address);
            ctx->sr &= ~(CARRY | ZERO | NEGATIVE);
            ctx->sr |= (ctx->ac & NEGATIVE) ? (CARRY | NEGATIVE): 0;
            ctx->sr |= ((!ctx->ac)? ZERO: 0);
            break;
        case ANE:
            // Unstable instruction.
            // A <- (A | const) & X & M
            // magic const set to 0xff but, could also be 0xee, 0xdd, ..., 0x00 depending on analog effects
            poll_interrupt(ctx);
            ctx->ac = ctx->x & read(ctx, address);
            set_ZN(ctx, ctx->ac);
            break;
        case ARR: {
            // Unstable instruction.
            // A <- A & (const | M), ROR A
            // magic const set to 0x00 but, could also be 0xff, 0xee, ..., 0x11 depending on analog effects
            poll_interrupt(ctx);
            uint8_t val = ctx->ac & read(ctx, address);
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
            poll_interrupt(ctx);
            uint8_t opr = read(ctx, address);
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
            poll_interrupt(ctx);
            ctx->ac = read(ctx, address);
            ctx->x = ctx->ac;
            set_ZN(ctx, ctx->ac);
            break;
        case SAX: {
            poll_interrupt(ctx);
            write(ctx, address, ctx->ac & ctx->x);
            break;
        }
        case DCP: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m--);
            write(ctx, address, m);
            poll_interrupt(ctx);
            uint16_t diff = ctx->ac - m;
            ctx->sr &= ~(CARRY | NEGATIVE | ZERO);
            ctx->sr |= !(diff & 0xFF00) ? CARRY: 0;
            fast_set_ZN(ctx, diff);
            break;
        }
        case ISB: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m++);
            poll_interrupt(ctx);
            write(ctx, address, m);
            uint16_t diff = ctx->ac - m - ((ctx->sr & CARRY) == 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (!(diff & 0xFF00)) ? CARRY : 0;
            ctx->sr |= ((ctx->ac ^ diff) & (~m ^ diff) & 0x80) ? OVERFLW: 0;
            ctx->ac = diff;
            fast_set_ZN(ctx, ctx->ac);
            break;
        }
        case RLA: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m);
            poll_interrupt(ctx);
            m = rot_l(ctx, m);
            write(ctx, address, m);
            ctx->ac &= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case RRA: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m);
            poll_interrupt(ctx);
            m = rot_r(ctx, m);
            write(ctx, address, m);
            uint16_t sum = ctx->ac + m + ((ctx->sr & CARRY) != 0);
            ctx->sr &= ~(CARRY | OVERFLW | NEGATIVE | ZERO);
            ctx->sr |= (sum & 0xFF00 ? CARRY : 0);
            ctx->sr |= ((ctx->ac ^ sum) & (m ^ sum) & 0x80) ? OVERFLW : 0;
            ctx->ac = sum;
            fast_set_ZN(ctx, sum);
            break;
        }
        case SLO: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m);
            poll_interrupt(ctx);
            m = shift_l(ctx, m);
            write(ctx, address, m);
            ctx->ac |= m;
            set_ZN(ctx, ctx->ac);
            break;
        }
        case SRE: {
            uint8_t m = read(ctx, address);
            // dummy write
            write(ctx, address, m);
            poll_interrupt(ctx);
            m = shift_r(ctx, m);
            write(ctx, address, m);
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
            poll_interrupt(ctx);
            write(ctx, address, val);
            break;
        }
        case SHX: {
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x: ctx->x & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            poll_interrupt(ctx);
            write(ctx, address, val);
            break;
        }
        case SHA: {
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x & ctx->ac: ctx->x & ctx->ac & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            poll_interrupt(ctx);
            write(ctx, address, val);
            break;
        }
        case SHS:
            ctx->sp = ctx->ac & ctx->x;
            const uint8_t val = ctx->state & DMA_OCCURRED ? ctx->x & ctx->ac: ctx->x & ctx->ac & ((ctx->raw_address >> 8) + 1);
            if (has_page_break(ctx->raw_address, address))
                // Instruction unstable, corrupt high byte of address
                address = (address & 0xff) | (val << 8);
            poll_interrupt(ctx);
            write(ctx, address, val);
            break;
        case LAS:
            poll_interrupt(ctx);
            ctx->ac = ctx->x = ctx->sp = read(ctx, address) & ctx->sp;
            set_ZN(ctx, ctx->ac);
            break;
        default:
            break;
    }
}

static uint16_t read_abs_address(c6502 *ctx, uint16_t offset){
    // 16-bit address is little endian so read lo then hi
    uint8_t lo = read(ctx, offset);
    uint8_t hi = read(ctx, offset + 1);
    return (hi << 8) | lo;
}

static uint16_t read_abs_address_(c6502 *ctx, uint16_t offset){
    // read 16-bit address with no side effects
    // 16-bit address is little endian so read lo then hi
    uint8_t lo = read_mem(ctx->memory, offset);
    uint8_t hi = read_mem(ctx->memory, offset + 1);
    return (hi << 8) | lo;
}

static uint16_t get_address(c6502* ctx){
    uint16_t addr, hi,lo;
    switch (ctx->instruction->mode) {
        case IMPL:
        case ACC:
        case NONE:
            poll_interrupt(ctx);
            // dummy read
            read(ctx, ctx->pc);
            ctx->raw_address = 0;
            return 0;
        case REL: {
            // handled by branch logic
            return 0;
        }
        case IMT:
            ctx->raw_address = ctx->pc + 1;
            return ctx->pc++;
        case ZPG:
            ctx->raw_address = ctx->pc + 1;
            return read(ctx, ctx->pc++);
        case ZPG_X:
            addr = ctx->raw_address = ctx->raw_address = read(ctx, ctx->pc++);
            // dummy read
            read(ctx, addr);
            return (addr + ctx->x) & 0xFF;
        case ZPG_Y:
            addr = ctx->raw_address = read(ctx, ctx->pc++);
            // dummy read
            read(ctx, addr);
            return (addr + ctx->y) & 0xFF;
        case ABS:
            lo = read(ctx, ctx->pc++);
            if (ctx->instruction->opcode == JMP)
                poll_interrupt(ctx);
            addr = ctx->raw_address = lo | read(ctx, ctx->pc++) << 8;
            return addr;
        case ABS_X:
            addr = ctx->raw_address = read_abs_address(ctx, ctx->pc);
            ctx->pc += 2;
            switch (ctx->instruction->opcode) {
                // these don't take into account absolute x page breaks
                case STA:case ASL:case DEC:case INC:case LSR:case ROL:case ROR:
                // unofficial
                case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case SHY:
                    // invalid read
                    read(ctx, (addr & 0xff00) | ((addr + ctx->x) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->x)) {
                        // invalid read
                        read(ctx, (addr & 0xff00) | ((addr + ctx->x) & 0xff));
                        // ctx->cycles++;
                    }
            }
            return addr + ctx->x;
        case ABS_Y:
            addr = ctx->raw_address = read_abs_address(ctx, ctx->pc);
            ctx->pc += 2;
            switch (ctx->instruction->opcode) {
                case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case NOP:
                case SHX:case SHA:case SHS:
                    // invalid read
                    read(ctx, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->y)) {
                        // invalid read
                        read(ctx, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                        // ctx->cycles++;
                    }
            }
            return addr + ctx->y;
        case IND:
            addr = ctx->raw_address = read_abs_address(ctx, ctx->pc);
            ctx->pc += 2;
            lo = read(ctx, addr);
            // handle a bug in 6502 hardware where if reading from $xxFF (page boundary) the
            // LSB is read from $xxFF as expected but the MSB is fetched from xx00
            poll_interrupt(ctx);
            hi = read(ctx, (addr & 0xFF00) | (addr + 1) & 0xFF);
            return (hi << 8) | lo;
        case IDX_IND:
            addr = read(ctx, ctx->pc++);
            // dummy read
            read(ctx, addr);
            addr = (addr + ctx->x) & 0xff;
            lo = read(ctx, addr);
            hi = read(ctx, (addr + 1) & 0xFF);
            addr = ctx->raw_address = (hi << 8) | lo;
            return addr;
        case IND_IDX:
            addr = read(ctx, ctx->pc++);
            lo = read(ctx, addr & 0xFF);
            hi = read(ctx, (addr + 1) & 0xFF);
            addr = ctx->raw_address = (hi << 8) | lo;
            switch (ctx->instruction->opcode) {
                case STA:case SLO:case RLA:case SRE:case RRA:case DCP:case ISB: case NOP:
                case SHA:
                    // invalid read
                    read(ctx, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                    break;
                default:
                    if(has_page_break(addr, addr + ctx->y)) {
                        // invalid read
                        read(ctx, (addr & 0xff00) | ((addr + ctx->y) & 0xff));
                        // ctx->cycles++;
                    }
            }
            return addr + ctx->y;
        case SPEC:
            // special case handled by instruction logic
            break;
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
    write(ctx, STACK_START + ctx->sp--, value);
}

static void push_address(c6502* ctx, uint16_t address){
    write(ctx, STACK_START + ctx->sp--, address >> 8);
    write(ctx, STACK_START + ctx->sp--, address & 0xFF);
}

static void push_address_(c6502* ctx, uint16_t address){
    // no side effects
    write_mem(ctx->memory, STACK_START + ctx->sp--, address >> 8);
    write_mem(ctx->memory, STACK_START + ctx->sp--, address & 0xFF);
}

static uint8_t pop(c6502* ctx){
    return read(ctx, STACK_START + ++ctx->sp);
}

static uint8_t pop_(c6502* ctx){
    return read_mem(ctx->memory, STACK_START + ++ctx->sp);
}

static uint16_t pop_address(c6502* ctx){
    uint16_t addr = read(ctx, STACK_START + ++ctx->sp);
    return addr | ((uint16_t)read(ctx, STACK_START + ++ctx->sp)) << 8;
}

static uint16_t pop_address_(c6502* ctx){
    // no side effects
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
