#pragma once

#include <stdint.h>

#include "utils.h"
#include "mapper.h"

#define VISIBLE_SCANLINES 240
#define VISIBLE_DOTS 256
#define NTSC_SCANLINES_PER_FRAME 261
#define PAL_SCANLINES_PER_FRAME 311
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

struct Emulator;

typedef struct PPU{
    size_t frames;
    uint32_t screen[VISIBLE_DOTS * VISIBLE_SCANLINES];
    uint8_t V_RAM[0x800];
    uint8_t OAM[256];
    uint8_t OAM_cache[8];
    uint8_t palette[0x20];
    uint8_t OAM_cache_len;
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    size_t dots;
    size_t scanlines;
    uint16_t scanlines_per_frame;

    uint16_t v;
    uint16_t t;
    uint8_t x;
    uint8_t w;
    uint8_t oam_address;
    uint8_t buffer;

    uint8_t render;
    uint8_t bus;

    struct Emulator* emulator;
    Mapper* mapper;
} PPU;

// ARGB8888 palette
static const uint32_t nes_palette_raw[64] = {
    0xff666666, 0xff002a88, 0xff1412a7, 0xff3b00a4, 0xff5c007e, 0xff6e0040, 0xff6c0600, 0xff561d00,
    0xff333500, 0xff0b4800, 0xff005200, 0xff004f08, 0xff00404d, 0xff000000, 0xff000000, 0xff000000,
    0xffadadad, 0xff155fd9, 0xff4240ff, 0xff7527fe, 0xffa01acc, 0xffb71e7b, 0xffb53120, 0xff994e00,
    0xff6b6d00, 0xff388700, 0xff0c9300, 0xff008f32, 0xff007c8d, 0xff000000, 0xff000000, 0xff000000,
    0xfffffeff, 0xff64b0ff, 0xff9290ff, 0xffc676ff, 0xfff36aff, 0xfffe6ecc, 0xfffe8170, 0xffea9e22,
    0xffbcbe00, 0xff88d800, 0xff5ce430, 0xff45e082, 0xff48cdde, 0xff4f4f4f, 0xff000000, 0xff000000,
    0xfffffeff, 0xffc0dfff, 0xffd3d2ff, 0xffe8c8ff, 0xfffbc2ff, 0xfffec4ea, 0xfffeccc5, 0xfff7d8a5,
    0xffe4e594, 0xffcfef96, 0xffbdf4ab, 0xffb3f3cc, 0xffb5ebf2, 0xffb8b8b8, 0xff000000, 0xff000000,
};


static uint32_t nes_palette[64];


void execute_ppu(PPU* ppu);
void reset_ppu(PPU* ppu);
void init_ppu(struct Emulator* emulator);
uint8_t read_status(PPU* ppu);
uint8_t read_ppu(PPU* ppu);
void set_ctrl(PPU* ppu, uint8_t ctrl);
void write_ppu(PPU* ppu, uint8_t value);
void dma(PPU* ppu, uint8_t value);
void set_scroll(PPU* ppu, uint8_t coord);
void set_address(PPU* ppu, uint8_t address);
void set_oam_address(PPU* ppu, uint8_t address);
uint8_t read_oam(PPU* ppu);
void write_oam(PPU* ppu, uint8_t value);
uint8_t read_vram(PPU* ppu, uint16_t address);
void write_vram(PPU* ppu, uint16_t address, uint8_t value);
