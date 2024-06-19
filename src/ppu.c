#include <string.h>

#include "ppu.h"
#include "utils.h"
#include "mmu.h"
#include "cpu6502.h"
#include "emulator.h"

static uint16_t render_background(PPU* ppu);
static uint16_t render_sprites(PPU* ppu, uint16_t bg_addr, uint8_t* back_priority);
uint32_t nes_palette[64];

void init_ppu(struct Emulator* emulator){
    to_pixel_format(nes_palette_raw, nes_palette, 64, SDL_PIXELFORMAT_ABGR8888);
    PPU* ppu = &emulator->ppu;
#if NAMETABLE_MODE
    ppu->screen = malloc(sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS * 4);
#else
    ppu->screen = malloc(sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS);
#endif
    ppu->emulator = emulator;
    ppu->mapper = &emulator->mapper;
    ppu->scanlines_per_frame = emulator->type == NTSC ? NTSC_SCANLINES_PER_FRAME : PAL_SCANLINES_PER_FRAME;

    memset(ppu->palette, 0, 0x20);
    memset(ppu->OAM_cache, 0, sizeof(ppu->OAM_cache)); //TODO: add sizeof in all memory operations
    memset(ppu->V_RAM, 0, 0x800);
    memset(ppu->OAM, 0, 256);
    ppu->oam_address = 0;
    ppu->v = 0;
    reset_ppu(ppu);
}

void reset_ppu(PPU* ppu){
    ppu->t = ppu->x = ppu->dots = 0;
    ppu->scanlines = 261;
    ppu->w = 1;
    ppu->ctrl &= ~0xFC;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->frames = 0;
    ppu->OAM_cache_len = 0;
    memset(ppu->OAM_cache, 0, 8);
    memset(ppu->screen, 0, sizeof(ppu->screen));
}

void exit_ppu(PPU* ppu) {
    if(ppu->screen != NULL) {
        free(ppu->screen);
    }
}

void set_address(PPU* ppu, uint8_t address){
    if(ppu->w){
        // first write
        ppu->t &= 0xff;
        ppu->t |= (address & 0x3f) << 8; // store only upto bit 14
        ppu->w = 0;
    }else{
        // second write
        ppu->t &= 0xff00;
        ppu->t |= address;
        ppu->v = ppu->t;
        ppu->w = 1;
    }
}


void set_oam_address(PPU* ppu, uint8_t address){
    ppu->oam_address = address;
}

uint8_t read_oam(PPU* ppu){
    return ppu->OAM[ppu->oam_address];
}

void write_oam(PPU* ppu, uint8_t value){
    ppu->OAM[ppu->oam_address++] = value;
}

void set_scroll(PPU* ppu, uint8_t coord){
    if(ppu->w){
        // first write
        ppu->t &= ~X_SCROLL_BITS;
        ppu->t |= (coord >> 3) & X_SCROLL_BITS;
        ppu->x = coord & 0x7;
        ppu->w = 0;
    }else{
        // second write
        ppu->t &= ~Y_SCROLL_BITS;
        ppu->t |= ((coord & 0x7) << 12) | ((coord & 0xF8) << 2);
        ppu->w = 1;
    }
}

uint8_t read_ppu(PPU* ppu){
    uint8_t prev_buff = ppu->buffer, data;
    ppu->buffer = read_vram(ppu, ppu->v);

    if(ppu->v >= 0x3F00)
        data = ppu->buffer;
    else
        data = prev_buff;
    ppu->v += ((ppu->ctrl & BIT_2) ? 32 : 1);
    return data;
}

void write_ppu(PPU* ppu, uint8_t value){
    write_vram(ppu, ppu->v, value);
    ppu->v += ((ppu->ctrl & BIT_2) ? 32 : 1);
}

void dma(PPU* ppu, uint8_t address){
    Memory* memory = &ppu->emulator->mem;
    uint8_t* ptr = get_ptr(memory, address * 0x100);
    if(ptr == NULL) {
        // Probably in PRG ROM so it is not possible to resolve a pointer
        // due to bank switching, so we do it the slow hard way
        for(int i = 0; i < 256; i++) {
            ppu->OAM[(ppu->oam_address + i) & 0xff] = read_mem(memory, address * 0x100 + i);
        }
    }else {
        // copy from OAM address to the end (256 bytes)
        memcpy(ppu->OAM + ppu->oam_address, ptr, 256 - ppu->oam_address);
        if(ppu->oam_address) {
            // wrap around and copy from start to OAM address if OAM is not 0x00
            memcpy(ppu->OAM, ptr + (256 - ppu->oam_address), ppu->oam_address);
        }
        // last value
        memory->bus = ptr[255];
    }
    ppu->emulator->cpu.dma_cycles += 513;
    // skip extra cycle on odd cycle
    ppu->emulator->cpu.dma_cycles += ppu->emulator->cpu.odd_cycle;
}



uint8_t read_vram(PPU* ppu, uint16_t address){
    address = address & 0x3fff;
    ppu->bus = address;

    if(address < 0x2000) {
        ppu->bus = ppu->mapper->read_CHR(ppu->mapper, address);
        return ppu->bus;
    }

    if(address < 0x3F00){
        address = (address & 0xefff) - 0x2000;
        ppu->bus = ppu->V_RAM[ppu->mapper->name_table_map[address / 0x400] + (address & 0x3ff)];
        return ppu->bus;
    }

    if(address < 0x4000)
        return ppu->palette[(address - 0x3F00) % 0x20];

    return 0;
}

void write_vram(PPU* ppu, uint16_t address, uint8_t value){
    address = address & 0x3fff;
    ppu->bus = value;

    if(address < 0x2000)
        ppu->mapper->write_CHR(ppu->mapper, address, value);
    else if(address < 0x3F00){
        address = (address & 0xefff) - 0x2000;
        ppu->V_RAM[ppu->mapper->name_table_map[address / 0x400] + (address & 0x3ff)] = value;
    }

    else if(address < 0x4000) {
        address = (address - 0x3F00) % 0x20;
        if(address % 4 == 0) {
            ppu->palette[address] = value;
            ppu->palette[address ^ 0x10] = value;
        }
        else
            ppu->palette[address] = value;
    }

}

uint8_t read_status(PPU* ppu){
    uint8_t status = ppu->status;
    ppu->w = 1;
    ppu->status &= ~BIT_7; // reset v_blank
    return status;
}

void set_ctrl(PPU* ppu, uint8_t ctrl){
    ppu->ctrl = ctrl;
    // set name table in temp address
    ppu->t &= ~0xc00;
    ppu->t |= (ctrl & BASE_NAMETABLE) << 10;
}

void execute_ppu(PPU* ppu){
    if(ppu->scanlines < VISIBLE_SCANLINES){
        // render scanlines 0 - 239
        if(ppu->dots > 0 && ppu->dots <= VISIBLE_DOTS){
            int x = (int)ppu->dots - 1;
            uint8_t fine_x = ((uint16_t)ppu->x + x) % 8, palette_addr = 0, palette_addr_sp = 0, back_priority = 0;

            if(ppu->mask & SHOW_BG){
                palette_addr = render_background(ppu);
                if(fine_x == 7) {
                    if ((ppu->v & COARSE_X) == 31) {
                        ppu->v &= ~COARSE_X;
                        // switch horizontal nametable
                        ppu->v ^= 0x400;
                    }
                    else
                        ppu->v++;
                }
            }
            if(ppu->mask & SHOW_SPRITE && ((ppu->mask & SHOW_SPRITE_8) || x >=8)){
                palette_addr_sp = render_sprites(ppu, palette_addr, &back_priority);
            }
            if((!palette_addr && palette_addr_sp) || (palette_addr && palette_addr_sp && !back_priority))
                palette_addr = palette_addr_sp;

            palette_addr = ppu->palette[palette_addr];
            ppu->screen[ppu->scanlines * VISIBLE_DOTS + ppu->dots - 1] = nes_palette[palette_addr];
        }
        if(ppu->dots == VISIBLE_DOTS + 1 && ppu->mask & SHOW_BG){
            if((ppu->v & FINE_Y) != FINE_Y) {
                // increment coarse x
                ppu->v += 0x1000;
            }
            else{
                ppu->v &= ~FINE_Y;
                uint16_t coarse_y = (ppu->v & COARSE_Y) >> 5;
                if(coarse_y == 29){
                    coarse_y = 0;
                    // toggle bit 11 to switch vertical nametable
                    ppu->v ^= 0x800;
                }
                else if(coarse_y == 31){
                    // nametable not switched
                    coarse_y = 0;
                }
                else{
                    coarse_y++;
                }

                ppu->v = (ppu->v & ~COARSE_Y) | (coarse_y << 5);
            }
        }
        else if(ppu->dots == VISIBLE_DOTS + 2 && (ppu->mask & RENDER_ENABLED)){
            ppu->v &= ~HORIZONTAL_BITS;
            ppu->v |= ppu->t & HORIZONTAL_BITS;
        }
        else if(ppu->dots == END_DOT){
            memset(ppu->OAM_cache, 0, 8);
            ppu->OAM_cache_len = 0;
            uint8_t range = ppu->ctrl & LONG_SPRITE ? 16: 8;
            for(size_t i = ppu->oam_address / 4; i < 64; i++){
                int diff = (int)ppu->scanlines - ppu->OAM[i * 4];
                if(diff >= 0 && diff < range){
                    ppu->OAM_cache[ppu->OAM_cache_len++] = i * 4;
                    if(ppu->OAM_cache_len >= 8)
                        break;
                }
            }
        }
    }
    else if(ppu->scanlines == VISIBLE_SCANLINES){
        // post render scanline 240/239
    }
    else if(ppu->scanlines < ppu->scanlines_per_frame){
        // v blanking scanlines 241 - 261/311
        if(ppu->dots == 1 && ppu->scanlines == VISIBLE_SCANLINES + 1){
            // set v-blank
            ppu->status |= V_BLANK;
            if(ppu->ctrl & GENERATE_NMI){
                // generate NMI
                interrupt(&ppu->emulator->cpu, NMI);
            }
        }
    }
    else{
        // pre-render scanline 262/312
        if(ppu->dots == 1){
            // reset v-blank and sprite zero hit
            ppu->status &= ~(V_BLANK | SPRITE_0_HIT);
        }
        else if(ppu->dots == VISIBLE_DOTS + 2 && (ppu->mask & RENDER_ENABLED)){
            ppu->v &= ~HORIZONTAL_BITS;
            ppu->v |= ppu->t & HORIZONTAL_BITS;
        }
        else if(ppu->dots > 280 && ppu->dots <= 304 && (ppu->mask & RENDER_ENABLED)){
            ppu->v &= ~VERTICAL_BITS;
            ppu->v |= ppu->t & VERTICAL_BITS;
        }
        else if(ppu->dots == END_DOT - 1 && ppu->frames & 1 && ppu->mask & RENDER_ENABLED && ppu->emulator->type == NTSC) {
            // skip one cycle on odd frames if rendering is enabled for NTSC
            ppu->dots++;
        }

        if(ppu->dots >= END_DOT) {
            // inform emulator to render contents of ppu on first dot
            ppu->render = 1;
            ppu->frames++;
        }
    }

    // increment dots and scanlines

    if(++ppu->dots >= DOTS_PER_SCANLINE) {
        if (ppu->scanlines++ >= ppu->scanlines_per_frame)
            ppu->scanlines = 0;
        ppu->dots = 0;
    }
}


static uint16_t render_background(PPU* ppu){
    int x = (int)ppu->dots - 1;
    uint8_t fine_x = ((uint16_t)ppu->x + x) % 8;

    if(!(ppu->mask & SHOW_BG_8) && x < 8)
        return 0;

    uint16_t tile_addr = 0x2000 | (ppu->v & 0xFFF);
    uint16_t attr_addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x38) | ((ppu->v >> 2) & 0x07);

    uint16_t pattern_addr = (read_vram(ppu, tile_addr) * 16 + ((ppu->v >> 12) & 0x7)) | ((ppu->ctrl & BG_TABLE) << 8);

    uint16_t palette_addr = (read_vram(ppu, pattern_addr) >> (7 ^ fine_x)) & 1;
    palette_addr |= ((read_vram(ppu, pattern_addr + 8) >> (7 ^ fine_x)) & 1) << 1;

    if(!palette_addr)
        return 0;

    uint8_t attr = read_vram(ppu, attr_addr);
    return palette_addr | (((attr >> ((ppu->v >> 4) & 4 | ppu->v & 2)) & 0x3) << 2);
}

static uint16_t render_sprites(PPU* restrict ppu, uint16_t bg_addr, uint8_t* restrict back_priority){
    // 4 bytes per sprite
    // byte 0 -> y index
    // byte 1 -> tile index
    // byte 2 -> render info
    // byte 3 -> x index
    int x = (int)ppu->dots - 1, y = (int)ppu->scanlines;
    uint16_t palette_addr = 0;
    uint8_t length = ppu->ctrl & LONG_SPRITE ? 16: 8;
    for(int j = 0; j < ppu->OAM_cache_len; j++) {
        int i = ppu->OAM_cache[j];
        uint8_t tile_x = ppu->OAM[i + 3];

        if (x - tile_x < 0 || x - tile_x >= 8)
            continue;

        uint16_t tile = ppu->OAM[i + 1];
        uint8_t tile_y = ppu->OAM[i] + 1;
        uint8_t attr = ppu->OAM[i + 2];
        int x_off = (x - tile_x) % 8, y_off = (y - tile_y) % length;

        if (!(attr & FLIP_HORIZONTAL))
            x_off ^= 7;
        if (attr & FLIP_VERTICAL)
            y_off ^= (length - 1);

        uint16_t tile_addr;

        if (ppu->ctrl & LONG_SPRITE) {
            y_off = y_off & 7 | ((y_off & 8) << 1);
            tile_addr = (tile >> 1) * 32 + y_off;
            tile_addr |= (tile & 1) << 12;
        } else {
            tile_addr = tile * 16 + y_off + (ppu->ctrl & SPRITE_TABLE ? 0x1000 : 0);
        }

        palette_addr = (read_vram(ppu, tile_addr) >> x_off) & 1;
        palette_addr |= ((read_vram(ppu, tile_addr + 8) >> x_off) & 1) << 1;

        if (!palette_addr)
            continue;

        palette_addr |= 0x10 | ((attr & 0x3) << 2);
        *back_priority = attr & BIT_5;

        // sprite hit evaluation

        if (!(ppu->status & SPRITE_0_HIT)
            && (ppu->mask & SHOW_BG)
            && i == 0
            && palette_addr
            && bg_addr
            && x < 255)
            ppu->status |= SPRITE_0_HIT;
        break;
    }
    return palette_addr;
}