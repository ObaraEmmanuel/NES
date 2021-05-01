#pragma once

#include <stdint.h>

#include "utils.h"
#include "mapper.h"

#define VISIBLE_SCANLINES 240
#define VISIBLE_DOTS 256
#define SCANLINES_PER_FRAME 261
#define DOTS_PER_SCANLINE 341
#define END_DOT 340

enum{
    BG_TABLE        = 1 << 4,
    SPRITE_TABLE    = 1 << 3,
    SHOW_BG_8       = 1 << 1,
    SHOW_SPRITE_8   = 1 << 2,
    SHOW_BG         = 1 << 3,
    SHOW_SPRITE     = 1 << 4,
    LONG_SPRITE     = 1 << 5,
    SPRITE_0_HIT    = 1 << 6,
    FLIP_HORIZONTAL = 1 << 6,
    FLIP_VERTICAL   = 1 << 7,
    V_BLANK         = 1 << 7,
    GENERATE_NMI    = 1 << 7,
    RENDER_ENABLED  = 0x18,
    BASE_NAMETABLE  = 0x3,
    FINE_Y          = 0x7000,
    COARSE_Y        = 0x3E0,
    COARSE_X        = 0x1F,
    VERTICAL_BITS   = 0x7BE0,
    HORIZONTAL_BITS = 0x41F,
    X_SCROLL_BITS   = 0x1f,
    Y_SCROLL_BITS   = 0x73E0
};

struct Memory;

typedef struct {
    size_t frames;
    uint32_t screen[VISIBLE_DOTS * VISIBLE_SCANLINES];
    uint8_t V_RAM[0x800];
    uint8_t OAM[256];
    uint8_t OAM_cache[8];
    uint8_t palette[0x20];
    uint8_t OAM_cache_len;
    uint8_t sprites;
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    size_t dots;
    size_t scanlines;

    uint16_t v;
    uint16_t t;
    uint8_t x;
    uint8_t w;
    uint8_t oam_address;
    uint8_t buffer;

    uint8_t render;

    Mapper* mapper;
    struct Memory* mem;

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
