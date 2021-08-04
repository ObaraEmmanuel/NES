#pragma once

#include "cpu6502.h"
#include "ppu.h"
#include "mmu.h"
#include "mapper.h"
#include "gfx.h"


// frame rate in Hz, anything above 60 for some reason will not work
#define FRAME_RATE 60

// turbo keys toggle rate (Hz)
// value should be a factor of FRAME_RATE
// and should never exceed FRAME_RATE for best result
#define TURBO_RATE 30

// sleep time when emulator is paused in milliseconds
#define IDLE_SLEEP 1000

static const uint64_t PERIOD = 1000 / FRAME_RATE;
static const uint16_t TURBO_SKIP = FRAME_RATE / TURBO_RATE;


typedef struct Emulator{
    struct c6502 cpu;
    struct PPU ppu;
    struct Memory mem;
    struct Mapper mapper;
    struct GraphicsContext g_ctx;

    uint32_t m_start, m_end;

    uint8_t exit;
    uint8_t pause;
} Emulator;


void init_emulator(struct Emulator* emulator, int argc, char *argv[]);
void run_emulator(struct Emulator* emulator);
void free_emulator(struct Emulator* emulator);
