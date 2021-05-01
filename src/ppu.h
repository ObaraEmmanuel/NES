#pragma once

#include <stdint.h>

#include "utils.h"
#include "mapper.h"

#define Y_SCROLL_BITS 0x73E0
#define X_SCROLL_BITS 0x1f
#define HORIZONTAL_BITS 0x41F
#define VERTICAL_BITS 0x7BE0
#define FINE_Y_SCROLL_BITS 0x7
#define COARSE_Y_SCROLL_BITS 0xF8

#define VISIBLE_SCAN_LINES 240
#define SCANLINE_VISIBLE_DOTS 256
#define FRAME_END_SCANLINE 261
#define SCANLINE_CYCLES 341
#define SCANLINE_END_CYCLE 340

typedef enum {
    PRE_RENDER,
    RENDER,
    POST_RENDER,
    V_BLANK
} PPUState;

struct Memory;

typedef struct {
    uint32_t screen[SCANLINE_VISIBLE_DOTS * VISIBLE_SCAN_LINES];
    uint8_t V_RAM[0x800];
    uint8_t OAM[256];
    uint8_t sprite_list[64];
    uint8_t palette[0x20];
    uint8_t sprites;
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t even_frame;
    size_t cycles;
    size_t scanlines;

    uint16_t v;
    uint16_t t;
    uint8_t x;
    uint8_t waiting_value;
    uint8_t oam_address;
    uint8_t buffer;

    uint8_t render;
    PPUState state;

    Mapper* mapper;
    struct Memory* mem;

    // internal use
    uint8_t render_flags; // bit: 0 - bg opaque 1 - sprite opaque 2 - sprite fg 3 -> 7 - palette addr

} PPU;


static const uint32_t nes_palette[64] = {
    0x666666ff, 0x002a88ff, 0x1412a7ff, 0x3b00a4ff, 0x5c007eff, 0x6e0040ff, 0x6c0600ff, 0x561d00ff,
    0x333500ff, 0x0b4800ff, 0x005200ff, 0x004f08ff, 0x00404dff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xadadadff, 0x155fd9ff, 0x4240ffff, 0x7527feff, 0xa01accff, 0xb71e7bff, 0xb53120ff, 0x994e00ff,
    0x6b6d00ff, 0x388700ff, 0x0c9300ff, 0x008f32ff, 0x007c8dff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xfffeffff, 0x64b0ffff, 0x9290ffff, 0xc676ffff, 0xf36affff, 0xfe6eccff, 0xfe8170ff, 0xea9e22ff,
    0xbcbe00ff, 0x88d800ff, 0x5ce430ff, 0x45e082ff, 0x48cddeff, 0x4f4f4fff, 0x000000ff, 0x000000ff,
    0xfffeffff, 0xc0dfffff, 0xd3d2ffff, 0xe8c8ffff, 0xfbc2ffff, 0xfec4eaff, 0xfeccc5ff, 0xf7d8a5ff,
    0xe4e594ff, 0xcfef96ff, 0xbdf4abff, 0xb3f3ccff, 0xb5ebf2ff, 0xb8b8b8ff, 0x000000ff, 0x000000ff,
};


void execute_ppu(PPU* ppu);
void reset_ppu(PPU* ppu);
void init_ppu(PPU* ppu);
uint8_t read_status(PPU* ppu);
uint8_t read_ppu(PPU* ppu);
void set_ctrl(PPU* ppu, uint8_t ctrl);
void write_ppu(PPU* ppu, uint8_t value);
void dma(PPU* ppu, struct Memory* memory, uint8_t value);
void set_scroll(PPU* ppu, uint8_t coord);
void set_address(PPU* ppu, uint8_t address);
void set_oam_address(PPU* ppu, uint8_t address);
uint8_t read_oam(PPU* ppu);
void write_oam(PPU* ppu, uint8_t value);
void show_tile(PPU* ppu, const uint8_t* bank, size_t tile);
