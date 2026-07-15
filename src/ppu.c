#include <string.h>
#include <stdlib.h>
#include "ppu.h"
#include "emulator.h"
#include "utils.h"
#include "mmu.h"
#include "cpu6502.h"

static uint16_t render_background(PPU* ppu);
static uint16_t render_sprites(PPU* ppu, uint16_t bg_addr, uint8_t* back_priority);
static void update_NMI(PPU* ppu, uint8_t delay);
static void evaluate_sprites(PPU* ppu);
static uint8_t get_bg_pixel(PPU *ppu);
static uint8_t get_sprite_pixel(PPU* ppu);
static uint8_t get_pixel(PPU* ppu);
static void inc_hori_v(PPU* ppu);
static void inc_vert_v(PPU* ppu);
uint32_t nes_palette[64];
static size_t screen_size;

void init_ppu(struct Emulator* emulator){
    to_pixel_format(nes_palette_raw, nes_palette, 64, ABGR8888);
    PPU* ppu = &emulator->ppu;
#if NAMETABLE_MODE
    screen_size = sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS * 4;
#else
    screen_size = sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS;
#endif
    ppu->screen = malloc(screen_size);
    ppu->emulator = emulator;
    ppu->mapper = &emulator->mapper;
    ppu->scanlines_per_frame = emulator->type == NTSC ? NTSC_SCANLINES_PER_FRAME : PAL_SCANLINES_PER_FRAME;

    memset(ppu->palette, 0, sizeof(ppu->palette));
    memset(ppu->OAM_cache, 0, sizeof(ppu->OAM_cache));
    memset(ppu->V_RAM, 0, sizeof(ppu->V_RAM));
    memset(ppu->OAM, 0, sizeof(ppu->OAM));
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
    ppu->render_state_delay = 0;
    ppu->render_status = 0;
    ppu->supress_vblank = 0;
    ppu->OAM_cache_len = 0;
    memset(ppu->OAM_cache, 0, 8);
    memset(ppu->screen, 0, screen_size);
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

    if(ppu->v >= 0x3F00) {
        data = ppu->buffer;
        // read underlying nametable mirrors into buffer
        // 0x3f00 - 0x3fff maps to 0x2f00 - 0x2fff
        ppu->buffer = read_vram(ppu, ppu->v & 0xefff);
    }else
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
    // halt CPU for DMA and skip extra cycle if on odd cycle
    do_DMA(&ppu->emulator->cpu, 513 + ppu->emulator->cpu.odd_cycle);
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
}



uint8_t read_vram(PPU* ppu, uint16_t address){
    address = address & 0x3fff;

    if(address < 0x2000) {
        ppu->bus = ppu->mapper->read_CHR(ppu->mapper, address);
        return ppu->bus;
    }

    if(address < 0x3F00){
        address = (address & 0xefff) - 0x2000;
        ppu->bus = ppu->V_RAM[ppu->mapper->name_table_map[address / 0x400] + (address & 0x3ff)];
        return ppu->bus;
    }

    if(address < 0x4000) {
        // palette RAM provide first 6 bits and remaining 2 bits are open bus
        uint8_t val = ppu->palette[(address - 0x3F00) % 0x20] & 0x3f | (ppu->bus & 0xc0);
        if (ppu->mask & 0x1)
            // greyscale mode; lower 4 bits = 0000
                return val & 0xf0;
        return val;
    }

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
    if (ppu->scanlines == 241 && (ppu->dots == 1 || ppu->dots == 2 || ppu->dots == 3))
        ppu->supress_vblank = 1;
    else
        update_NMI(ppu, 0);
    return status;
}

void set_ctrl(PPU* ppu, uint8_t ctrl){
    ppu->ctrl = ctrl;
    update_NMI(ppu, 1);
    // set name table in temp address
    ppu->t &= ~0xc00;
    ppu->t |= (ctrl & BASE_NAMETABLE) << 10;
}

void set_mask(PPU* ppu, uint8_t mask) {
    uint8_t should_render = (mask & RENDER_BITS) > 0;
    if (should_render != ppu->render_status) {
        ppu->render_state_delay = 3;
    }
    ppu->mask = mask;
}

static void update_NMI(PPU* ppu, uint8_t delay) {
    if (delay) {
        ppu->nmi_delay = delay;
        return;
    }
    if(ppu->ctrl & BIT_7 && ppu->status & BIT_7 && !ppu->supress_vblank)
        interrupt(&ppu->emulator->cpu, NMI);
    else
        interrupt_clear(&ppu->emulator->cpu, NMI);
    ppu->supress_vblank = 0;
}

static void inc_hori_v(PPU* ppu) {
    // inc horizontal V
    if ((ppu->v & COARSE_X) == 31) {
        ppu->v &= ~COARSE_X;
        // switch horizontal nametable
        ppu->v ^= 0x400;
    } else ppu->v++;
}

static void inc_vert_v(PPU* ppu) {
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

static uint8_t get_bg_pixel(PPU *ppu) {
    if (!(ppu->mask & SHOW_BG))
        return 0;

    if(!(ppu->mask & SHOW_BG_8) && ppu->dots <= 8)
        return 0;

    const PictureUnit* pu = &ppu->p_unit;
    const uint16_t bit_mux = 0x8000 >> ppu->x;
    const uint8_t pattern = (pu->pattern_LSB & bit_mux ? 1 : 0) | (pu->pattern_MSB & bit_mux ? 0b10 : 0);
    const uint8_t palette = (pu->attr_LSB & bit_mux ? 1 : 0) | (pu->attr_MSB & bit_mux ? 0b10 : 0);

    if (!pattern)
        return 0;

    return palette << 2 | pattern;
}

static uint8_t get_sprite_pixel(PPU* ppu) {
    if (ppu->scanlines == 0)
        return 0;
    uint8_t pattern = 0;
    SpriteUnit* priority_unit = NULL;
    for (int i = 0; i < 8; i++) {
        SpriteUnit* unit = &ppu->sprite_units[i];
        if (unit->x != 0) {
            unit->x--;
            continue;
        }

        uint8_t pattern_bits = 0;
        if (unit->attr & FLIP_HORIZONTAL) {
            pattern_bits = unit->pattern_LSB & 1 | (unit->pattern_MSB & 1) << 1;
            unit->pattern_LSB >>= 1;
            unit->pattern_MSB >>= 1;
        } else {
            pattern_bits = unit->pattern_LSB >> 7 | (unit->pattern_MSB >> 7) << 1;
            unit->pattern_LSB <<= 1;
            unit->pattern_MSB <<= 1;
        }
        if (pattern_bits && priority_unit == NULL) {
            priority_unit = unit;
            pattern = pattern_bits;
        }
    }

    if (!(ppu->mask & SHOW_SPRITE))
        return 0;

    if (!(ppu->mask & SHOW_SPRITE_8) && ppu->dots <= 8)
        return 0;

    if (priority_unit == NULL)
        return 0;

    return 0x10 | (priority_unit->attr & 3) << 2 | pattern | priority_unit->attr & BIT_5 | (priority_unit->attr & BIT_2 ? BIT_6 : 0);
}

static uint8_t get_pixel(PPU* ppu) {
    uint8_t sprite_pixel = get_sprite_pixel(ppu);
    uint8_t bg_pixel = get_bg_pixel(ppu);

    switch ((bg_pixel > 0) << 1 | (sprite_pixel > 0)) {
        case 0b00:
            // this would be connected tied to EXT pins
            // for now let's tie them to ground
            return 0;
        case 0b01:
            // keep only the 5 relevant bits
            return sprite_pixel & 0x1f;
        case 0b10:
            return bg_pixel;
        case 0b11:
            if (sprite_pixel & BIT_6 && ppu->dots < 256)
                ppu->status |= SPRITE_0_HIT;

            if (sprite_pixel & BIT_5)
                return bg_pixel;
            return sprite_pixel & 0x1f;
        default:
            return 0;
    }
}

static void evaluate_sprites(PPU* ppu) {
    // sprite evaluation
    SpriteEvalMachine* su = &ppu->sprite_eval_unit;

    // finish OAM clear and start evaluation
    if (ppu->dots == 65) {
        // OAM clear complete, move on to sprite evaluation
        su->state = READ_OAM_Y;
        su->n = su->m = 0;
        su->sec_oam_index = 0;
    }

    if (ppu->dots == 1) {
        // reset sprite evaluation machine
        ppu->sprite_eval_unit.state = CLEAR_OAM_READ;
        ppu->sprite_eval_unit.sec_oam_index = 0;
    }

    switch (su->state) {
        case CLEAR_OAM_READ:
            // (even dot) delay 1 cycle
            su->state = CLEAR_OAM_WRITE;
            break;
        case CLEAR_OAM_WRITE:
            // (odd dot) write to secondary OAM
            ppu->OAM_cache[su->sec_oam_index++] = 0xff;
            su->state = CLEAR_OAM_READ;
            break;
        case READ_OAM_Y:
            if (su->n >= 64) {
                su->state = OAM_EOF;
                break;
            }
            su->buffer = ppu->OAM[su->n << 2 | su->m];
            su->state = CMP_OAM_Y;
            break;
        case CMP_OAM_Y: {
            uint8_t is_within_y = ppu->scanlines - su->buffer < 8 << (ppu->ctrl & LONG_SPRITE? 1 : 0);
            if (su->sec_oam_index < 32) {
                ppu->OAM_cache[su->sec_oam_index] = su->buffer;
                if (is_within_y) {
                    su->sec_oam_index++;
                    su->state = READ_BYTE;
                    // read the next 3 bytes into secondary OAM
                    su->m++;
                    su->remaining = 3;
                } else {
                    su->state = READ_OAM_Y;
                    su->n++;
                    // not necessary but just in case
                    su->m = 0;
                }
            } else {
                // sprite overflow
                if (is_within_y) {
                    ppu->status |= SPRITE_OVERFLOW;
                    su->state = READ_BYTE;
                    // read the next 3 bytes
                    su->m++;
                    su->remaining = 3;
                } else {
                    su->state = READ_OAM_Y;
                    // incorrectly increment both n and m
                    su->m = (su->m + 1) & 3;
                    su->n++;
                }
            }
            break;
        }
        case READ_BYTE:
            if (su->n >= 64) {
                su->state = OAM_EOF;
                break;
            }
            su->buffer = ppu->OAM[su->n << 2 | su->m++];
            // use unimplemented bit 2 of attr byte to mark sprite 0
            if (su->remaining == 2) {
                su->buffer &= ~BIT_2;
                if (su->n == 0)
                    su->buffer |= BIT_2;
            }
            if (su->m > 3) {
                su->m = 0;
                su->n++;
            }
            su->state = WRITE_BYTE;
            break;
        case WRITE_BYTE:
            if (su->sec_oam_index < 32)
                ppu->OAM_cache[su->sec_oam_index++] = su->buffer;
            if (--su->remaining == 0)
                su->state = READ_OAM_Y;
            else
                su->state = READ_BYTE;
            break;
        case OAM_EOF:
            break;
    }
}

static void fetch_frame(PPU* ppu) {
    PPUPhase phase = (ppu->dots - 1) & 7;
    PictureUnit* pu = &ppu->p_unit;

    uint8_t sprite_prefetch = ppu->dots > 256 && ppu->dots <= 320;
    uint8_t post_visible = ppu->dots > 256 && ppu->dots <= 320;
    uint8_t pre_render = ppu->dots >= 337;

    // clock bg output shift registers
    if (ppu->dots <= 256 || (ppu->dots > 320 && ppu->dots < 337)) {
        pu->pattern_LSB <<= 1;
        pu->pattern_MSB <<= 1;
        pu->attr_LSB <<= 1;
        pu->attr_MSB <<= 1;
    }

    switch (phase) {
        case NT_ADDR: // 0
            if (ppu->dots == 257) {
                ppu->v &= ~HORIZONTAL_BITS;
                ppu->v |= ppu->t & HORIZONTAL_BITS;
            }
            if (ppu->dots == 1) {
                // clear secondary OAM
                memset(ppu->OAM_cache, 0, 8);
                ppu->OAM_cache_len = 0;
            }
            // load NT address
            pu->fetch_addr = 0x2000 | ppu->v & 0xFFF;
            break;
        case NT_READ: // 1
            // load NT byte;
            pu->NT = read_vram(ppu, pu->fetch_addr);
            break;
        case AT_ADDR: // 2
            if (sprite_prefetch || pre_render)
                // load NT address
                pu->fetch_addr = 0x2000 | ppu->v & 0xFFF;
            else
                // load AT address
                pu->fetch_addr = 0x23C0 | ppu->v & 0x0C00 | ppu->v >> 4 & 0x38 | ppu->v >> 2 & 0x07;
            break;
        case AT_READ: // 3
            // load AT/NT byte
            pu->AT = read_vram(ppu, pu->fetch_addr);
            pu->AT >>= ppu->v >> 4 & 4 | ppu->v & 2;
            break;
        case BG_LSB_ADDR: // 4
            if (sprite_prefetch) {
                // load sprite LSB addr
                uint8_t sprite_index = (ppu->dots - 257) >> 3;
                SpriteUnit* unit = ppu->sprite_units + sprite_index;
                Sprite* sprite = (Sprite*)ppu->OAM_cache + sprite_index;

                unit->x = sprite->x;
                unit->attr = sprite->attr;

                uint8_t offset = ppu->scanlines - sprite->y;
                if (ppu->ctrl & LONG_SPRITE) {
                    // 8x16 sprite
                    uint16_t bank = sprite->tile & 1 ? 0x1000: 0;
                    uint8_t tile = sprite->tile & 0xfe;
                    uint8_t row = sprite->attr & FLIP_VERTICAL ? 15 - offset : offset;

                    if (row >= 8) {
                        tile++;
                        row -= 8;
                    }

                    pu->fetch_addr = bank + tile * 16 + row;
                } else {
                    // 8x8 sprite
                    uint16_t bank = ppu->ctrl & SPRITE_TABLE ? 0x1000: 0;
                    uint8_t row = sprite->attr & FLIP_VERTICAL ? 7 - offset : offset;

                    pu->fetch_addr = bank + sprite->tile * 16 + row;
                }
            } else {
                // load BG LSB addr
                pu->fetch_addr = pu->NT << 4 | ppu->v >> 12 & 0x7 | (ppu->ctrl & BG_TABLE) << 8;
            }
            break;
        case BG_LSB_READ: // 5
            if (sprite_prefetch) {
                // load sprite LSB byte
                uint8_t sprite_index = (ppu->dots - 257) >> 3;
                SpriteUnit* unit = ppu->sprite_units + sprite_index;
                unit->pattern_LSB = read_vram(ppu, pu->fetch_addr);
            }else {
                // load BG LSB byte
                pu->BG_LSB = read_vram(ppu, pu->fetch_addr);
            }
            break;
        case BG_MSB_ADDR: // 6
            // load BG MSB / sprite MSB addr
            pu->fetch_addr = pu->fetch_addr + 8;
            break;
        case BG_MSB_READ: // 7
            if (sprite_prefetch) {
                // load sprite MSB byte
                uint8_t sprite_index = (ppu->dots - 257) >> 3;
                SpriteUnit* unit = ppu->sprite_units + sprite_index;
                unit->pattern_MSB = read_vram(ppu, pu->fetch_addr);
            }else {
                // load BG MSB byte
                pu->BG_MSB = read_vram(ppu, pu->fetch_addr);
            }

            if (ppu->dots == 256) {
                inc_vert_v(ppu);
            }

            if (ppu->dots <= 256 || ppu->dots > 320) {
                inc_hori_v(ppu);
            }

            // shift relevant data for rendering
            pu->pattern_LSB = pu->pattern_LSB & 0xff00 | pu->BG_LSB;
            pu->pattern_MSB = pu->pattern_MSB & 0xff00 | pu->BG_MSB;
            pu->attr_LSB = pu->attr_LSB & 0xff00 | (pu->AT & 0b01 ? 0xff : 0) ;
            pu->attr_MSB = pu->attr_MSB & 0xff00 | (pu->AT & 0b10 ? 0xff : 0) ;

            break;
        default:
            // we should never get here
            break;
    }
}

void execute_ppu(PPU* ppu) {
    if (ppu->render_status && (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->scanlines_per_frame)) {
        if (ppu->dots == 0) {
            // do dot 0 stuff
        } else {
            // dots 1 - 256, scanline 0 - 239 (render region)
            if (ppu->scanlines < VISIBLE_SCANLINES && ppu->dots <= VISIBLE_DOTS) {
                evaluate_sprites(ppu);
                // only consider 6-bits from palette RAM since upper 2 bits are open bus
                uint8_t pixel = read_vram(ppu, 0x3f00 + get_pixel(ppu)) & 0x3f;
                // output pixel to output buffer
                ppu->screen[ppu->scanlines * 256 + ppu->dots - 1] = nes_palette[pixel];
            }
            // tile and attr pre-fetch
            fetch_frame(ppu);
        }
    }

    if (ppu->scanlines == VISIBLE_SCANLINES) {
        // post-render scanline
    }

    if(ppu->scanlines == 241 && ppu->dots == 1) {
        // set v-blank
        if (!ppu->supress_vblank)
            ppu->status |= V_BLANK;
        ppu->supress_vblank = 0;
        update_NMI(ppu, 3);
    }

    if (ppu->scanlines == ppu->scanlines_per_frame) {
        // pre-render scanline 262/312
        if(ppu->dots == 1){
            // reset v-blank and sprite zero hit
            ppu->status &= ~(V_BLANK | SPRITE_0_HIT | SPRITE_OVERFLOW);
            update_NMI(ppu, 0);
        }
        else if(ppu->dots == 257 && ppu->render_status){
            ppu->v &= ~HORIZONTAL_BITS;
            ppu->v |= ppu->t & HORIZONTAL_BITS;
        }
        else if(ppu->dots == 260 && ppu->render_status) {
            ppu->mapper->on_scanline(ppu->mapper);
        }
        else if(ppu->dots > 280 && ppu->dots <= 304 && ppu->render_status){
            ppu->v &= ~VERTICAL_BITS;
            ppu->v |= ppu->t & VERTICAL_BITS;
        }
        else if(ppu->dots == 339 && ppu->frames & 1 && ppu->render_status && ppu->emulator->type == NTSC) {
            // skip one cycle on odd frames if rendering is enabled for NTSC
            ppu->dots++;
        }

        if(ppu->dots >= 340) {
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

    // update render status
    if (ppu->render_state_delay) {
        ppu->render_state_delay--;
        if(ppu->render_state_delay == 0) {
            ppu->render_status = (ppu->mask & RENDER_BITS) > 0;
        }
    }

    //update NMI delay
    if (ppu->nmi_delay) {
        ppu->nmi_delay--;
        if(ppu->nmi_delay == 0) {
            update_NMI(ppu, 0);
        }
    }
}

void execute_ppu_old(PPU* ppu){
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
        else if(ppu->dots == VISIBLE_DOTS + 2 && ppu->render_status){
            ppu->v &= ~HORIZONTAL_BITS;
            ppu->v |= ppu->t & HORIZONTAL_BITS;
        }
        else if(ppu->dots == VISIBLE_DOTS + 4 && ppu->render_status) {
            ppu->mapper->on_scanline(ppu->mapper);
        }
        else if(ppu->dots == 320 && ppu->render_status){
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
            if (!ppu->supress_vblank)
                ppu->status |= V_BLANK;
            ppu->supress_vblank = 0;
            update_NMI(ppu, 3);
        }
    }
    else{
        // pre-render scanline 262/312
        if(ppu->dots == 1){
            // reset v-blank and sprite zero hit
            ppu->status &= ~(V_BLANK | SPRITE_0_HIT);
            update_NMI(ppu, 0);
        }
        else if(ppu->dots == VISIBLE_DOTS + 2 && ppu->render_status){
            ppu->v &= ~HORIZONTAL_BITS;
            ppu->v |= ppu->t & HORIZONTAL_BITS;
        }
        else if(ppu->dots == VISIBLE_DOTS + 4 && ppu->render_status) {
            ppu->mapper->on_scanline(ppu->mapper);
        }
        else if(ppu->dots > 280 && ppu->dots <= 304 && ppu->render_status){
            ppu->v &= ~VERTICAL_BITS;
            ppu->v |= ppu->t & VERTICAL_BITS;
        }
        else if(ppu->dots == (END_DOT - 1) && ppu->frames & 1 && ppu->render_status && ppu->emulator->type == NTSC) {
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

    // update render status
    if (ppu->render_state_delay) {
        ppu->render_state_delay--;
        if(ppu->render_state_delay == 0) {
            ppu->render_status = (ppu->mask & RENDER_BITS) > 0;
        }
    }

    //update NMI delay
    if (ppu->nmi_delay) {
        ppu->nmi_delay--;
        if(ppu->nmi_delay == 0) {
            update_NMI(ppu, 0);
        }
    }
}


static uint16_t render_background(PPU* ppu){
    // old background rendering
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
    // old sprite rendering
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
