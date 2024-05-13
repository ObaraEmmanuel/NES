#pragma once

#include "cpu6502.h"
#include "ppu.h"
#include "mmu.h"
#include "apu.h"
#include "mapper.h"
#include "gfx.h"
#include "timers.h"


// frame rate in Hz
// I've slightly sped these up to minimize audio drop-outs
#define NTSC_FRAME_RATE 63
#define PAL_FRAME_RATE 53

// turbo keys toggle rate (Hz)
// value should be a factor of FRAME_RATE
// and should never exceed FRAME_RATE for best result
#define NTSC_TURBO_RATE 31.5f
#define PAL_TURBO_RATE 26.5f

// sleep time when emulator is paused in milliseconds
#define IDLE_SLEEP 1000


typedef struct Emulator{
    struct c6502 cpu;
    struct PPU ppu;
    struct APU apu;
    struct Memory mem;
    struct Mapper mapper;
    struct GraphicsContext g_ctx;
    struct Timer timer;

    TVSystem type;

    double time_diff;

    uint8_t exit;
    uint8_t pause;
} Emulator;


void init_emulator(struct Emulator* emulator, int argc, char *argv[]);
void run_emulator(struct Emulator* emulator);
void free_emulator(struct Emulator* emulator);
