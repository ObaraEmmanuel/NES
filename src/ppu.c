#include <string.h>
#include <stdlib.h>
#include "ppu.h"
#include "emulator.h"
#include "utils.h"
#include "cpu6502.h"

static void update_NMI(PPU* ppu, uint8_t delay);
static uint8_t is_y_in_range(PPU* ppu, uint8_t y);
static void clear_oam(PPU* ppu);
static void evaluate_sprites(PPU* ppu);
static uint8_t get_bg_pixel(PPU *ppu);
static uint8_t get_sprite_pixel(PPU* ppu);
static uint8_t get_pixel(PPU* ppu);
static void inc_hori_v(PPU* ppu);
static void inc_vert_v(PPU* ppu);
static void inc_v(PPU* ppu);
static void inc_sec_oam_addr(PPU* ppu);
uint32_t nes_palette[64];
static size_t screen_size;

void init_ppu(struct Emulator* emulator){
    to_pixel_format(nes_palette_raw, nes_palette, 64, ABGR8888);
    PPU* ppu = &emulator->ppu;
    memset(ppu, 0, sizeof(PPU));
#if NAMETABLE_MODE
    screen_size = sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS * 4;
#else
    screen_size = sizeof(uint32_t) * VISIBLE_SCANLINES * VISIBLE_DOTS;
#endif
    ppu->screen = malloc(screen_size);
    ppu->emulator = emulator;
    ppu->mapper = &emulator->mapper;
    ppu->pre_render = emulator->type == NTSC ? NTSC_SCANLINES_PER_FRAME : PAL_SCANLINES_PER_FRAME;
    reset_ppu(ppu);
}

void reset_ppu(PPU* ppu){
    ppu->t = ppu->x = 0;
    ppu->should_inc_vert_v = ppu->should_inc_hori_v = 0;
    ppu->dots = 1;
    ppu->scanlines = 261;
    ppu->w = 0;
    ppu->ctrl &= ~0xFC;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->frames = 0;
    ppu->render_state_delay = 0;
    ppu->render_status = 0;
    ppu->supress_vblank = 0;
    memset(ppu->OAM_cache, 0, sizeof(ppu->OAM_cache));
    memset(ppu->OAM, 0, sizeof(ppu->OAM));
}

void exit_ppu(PPU* ppu) {
    if(ppu->screen != NULL) {
        free(ppu->screen);
    }
}

void set_latch(PPU* ppu, uint8_t value, uint8_t mask) {
    ppu->latch &= ~mask;
    ppu->latch |= value & mask;
    // if not all bits are driven, don't refresh decay if it is not complete yet
    if (mask == 0xff || ppu->latch_decay == 0)
        ppu->latch_decay = 60000;
}

void set_address(PPU* ppu, uint8_t address){
    if(!ppu->w){
        // first write
        ppu->t &= 0xff;
        ppu->t |= (address & 0x3f) << 8; // store only upto bit 14
        ppu->w = 1;
    }else{
        // second write
        ppu->t &= 0xff00;
        ppu->t |= address;
        ppu->v = ppu->t;
        ppu->mapper->set_bus(ppu->mapper, ppu->v);
        ppu->w = 0;
    }
}

void set_oam_address(PPU* ppu, uint8_t address){
    ppu->oam_address = address;
}

uint8_t read_oam(PPU* ppu){
    if (ppu->render_status && (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->pre_render)) {
        return ppu->sprite_eval_unit.buffer;
    }
    // bits 2-4 of the attr byte (byte 2 of 4) are unimplemented and always read back as 0
    return ppu->OAM[ppu->oam_address] & ((ppu->oam_address & 0x03) == 0x02 ? 0xE3 : 0xFF);
}

void write_oam(PPU* ppu, uint8_t value){
    if (ppu->render_status && (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->pre_render)) {
        // glitchy OAM increment bumping only the upper 6 bits
        ppu->oam_address += 4;
        ppu->oam_address &= 0xfc;
        return;
    }
    ppu->OAM[ppu->oam_address++] = value;
}

void set_scroll(PPU* ppu, uint8_t coord){
    if(!ppu->w){
        // first write
        ppu->t &= ~X_SCROLL_BITS;
        ppu->t |= (coord >> 3) & X_SCROLL_BITS;
        ppu->x = coord & 0x7;
        ppu->w = 1;
    }else{
        // second write
        ppu->t &= ~Y_SCROLL_BITS;
        ppu->t |= ((coord & 0x7) << 12) | ((coord & 0xF8) << 2);
        ppu->w = 0;
    }
}

uint8_t read_ppu(PPU* ppu){
    uint8_t data = ppu->read_buffer;
    if (ppu->v >= 0x3F00)
        // read palette value immediately since it exists internally in PPU
        data = read_vram(ppu, ppu->v);

    // set-up buffer after 4 ppu clocks
    ppu->read_to_buffer = 4;
    return data;
}

void write_ppu(PPU* ppu, uint8_t value){
    // write to VRAM after 4 ppu clocks
    ppu->write_to_buffer = 4;
    ppu->write_val = value;
}

void dma(PPU* ppu, uint8_t address){
    schedule_dma(
        &ppu->emulator->cpu,
        DMA_OAM,
        address * 0x100,
        ppu->OAM,
        256,
        ppu->oam_address
    );
}

uint8_t read_vram(PPU* ppu, uint16_t address){
    address = address & 0x3fff;

    if(address < 0x2000) {
        return ppu->mapper->read_CHR(ppu->mapper, address);
    }

    if(address < 0x3F00){
        address = (address & 0xefff) - 0x2000;
        return ppu->V_RAM[ppu->mapper->name_table_map[address / 0x400] + (address & 0x3ff)];
    }

    if(address < 0x4000) {
        // palette RAM provide first 6 bits and remaining 2 bits are open bus
        uint8_t val = ppu->palette[(address - 0x3F00) % 0x20] & 0x3f | (ppu->latch & 0xc0);
        if (ppu->mask & 0x1)
            // greyscale mode; lower 4 bits = 0000
            return val & 0xf0;
        return val;
    }

    return 0;
}

void write_vram(PPU* ppu, uint16_t address, uint8_t value){
    address = address & 0x3fff;

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
    ppu->w = 0;
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

static void inc_v(PPU* ppu) {
    if (ppu->render_status &&  (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->pre_render))
        // glitchy increment of both vertical and horizontal
        ppu->should_inc_vert_v = ppu->should_inc_hori_v = 1;
    else
        ppu->should_inc_v = 1;
}

static void inc_sec_oam_addr(PPU* ppu) {
    if (ppu->sec_oam_address < 32)
        ppu->sec_oam_address++;
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
    uint8_t pattern = 0;
    SpriteUnit* priority_unit = NULL;
    for (int i = 0; i < 8; i++) {
        SpriteUnit* unit = &ppu->sprite_units[i];
        if (unit->halted || unit->x == 0) {
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

static uint8_t is_y_in_range(PPU* ppu, uint8_t y) {
    return (ppu->scanlines & 0xff) - y < 8 << (ppu->ctrl & LONG_SPRITE? 1 : 0);
}

static void clear_oam(PPU* ppu) {
    if (ppu->dots & 1)
        // writes happen on odd dots since we have the 1 dot ppu delay
        ppu->OAM_cache[ppu->sec_oam_address & 0x1f] = 0xff;
    else
        inc_sec_oam_addr(ppu);

    ppu->sprite_eval_unit.buffer = 0xff;
}

static void evaluate_sprites(PPU* ppu) {
    // sprite evaluation
    SpriteEvalMachine* su = &ppu->sprite_eval_unit;
    uint16_t addr = 0;

    // finish OAM clear and start evaluation
    if (ppu->dots == 65) {
        // OAM clear complete, move on to sprite evaluation
        su->state = READ_OAM_Y;
        su->n = su->m = 0;
        su->oam_addr = ppu->oam_address;
        su->has_sprite_zero = 0;
        su->has_overflown = 0;
    }

    switch (su->state) {
        case READ_OAM_Y:
            addr = (su->n << 2 | su->m) + su->oam_addr;
            if (addr >= 256) {
                su->state = OAM_EOF;
                su->buffer = ppu->OAM[0];
                su->n = 1;
                break;
            }
            su->buffer = ppu->OAM[addr] & ((addr & 0x03) == 0x02 ? 0xE3 : 0xFF);
            su->state = CMP_OAM_Y;
            break;
        case CMP_OAM_Y:
            if (ppu->sec_oam_address < 32) {
                ppu->OAM_cache[ppu->sec_oam_address] = su->buffer;
                if (is_y_in_range(ppu, su->buffer)) {
                    ppu->sec_oam_address++;
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
                if (is_y_in_range(ppu, su->buffer)) {
                    ppu->status |= SPRITE_OVERFLOW;
                    su->has_overflown = 1;
                    su->state = READ_BYTE;
                    // read the next 3 bytes
                    su->m++;
                    if (su->m > 3) {
                        su->m = 0;
                        su->n++;
                    }
                    su->remaining = 3;
                } else {
                    su->state = READ_OAM_Y;
                    // incorrectly increment both n and m
                    su->m = (su->m + 1) & 3;
                    su->n++;
                }
                // sec OAM writes converted to reads if it is full
                // sec OAM address points to 0 when full
                su->buffer = ppu->OAM_cache[0];
            }
            break;
        case READ_BYTE:
            addr = (su->n << 2 | su->m++) + su->oam_addr;
            if (addr >= 256) {
                su->state = OAM_EOF;
                su->buffer = ppu->OAM[0];
                su->n = 1;
                break;
            }
            // bits 2 - 4 of attr byte are unimplemented and should be zero
            su->buffer = ppu->OAM[addr] & ((addr & 0x03) == 0x02 ? 0xE3 : 0xFF);
            if (su->n == 0 && su->remaining == 3) {
                su->has_sprite_zero = 1;
            }
            if (su->m > 3) {
                su->m = 0;
                su->n++;
            }
            su->state = WRITE_BYTE;
            break;
        case WRITE_BYTE:
            if (ppu->sec_oam_address < 32)
                ppu->OAM_cache[ppu->sec_oam_address++] = su->buffer;
            else
                // sec OAM writes converted to reads from its last address
                su->buffer = ppu->OAM_cache[0];

            if (--su->remaining == 0) {
                if (su->has_overflown)
                    su->state = OAM_EOF;
                else
                    su->state = READ_OAM_Y;
            } else
                su->state = READ_BYTE;
            break;
        case OAM_EOF:
            if(ppu->dots & 1){
                addr = (su->n++ << 2) + su->oam_addr;
                su->buffer = ppu->OAM[addr & 0xff];
            } else {
                // sec OAM writes converted to reads from its last address
                su->buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
            }
            break;
    }
}

static void fetch_frame(PPU* ppu) {
    PPUPhase phase = (ppu->dots - 1) & 7;
    PictureUnit* pu = &ppu->p_unit;

    uint8_t sprite_prefetch = ppu->dots > 256 && ppu->dots <= 320;
    uint8_t pre_render = ppu->dots >= 337;

    // clock bg output shift registers
    if (ppu->dots <= 256 || (ppu->dots > 320 && ppu->dots < 337)) {
        pu->pattern_LSB = pu->pattern_LSB << 1 | 1;
        pu->pattern_MSB = pu->pattern_MSB << 1 | 1;
        pu->attr_LSB  = pu->attr_LSB << 1 | 1;
        pu->attr_MSB = pu->attr_MSB << 1 | 1;
    }

    if (sprite_prefetch) {
        ppu->oam_address = 0;
    }
    if (ppu->dots > 320) {
        ppu->sprite_eval_unit.buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
    }

    switch (phase) {
        case NT_ADDR: // 0
            // load NT address
            ppu->bus = 0x2000 | ppu->v & 0xFFF;
            if (ppu->dots == 257) {
                ppu->v &= ~HORIZONTAL_BITS;
                ppu->v |= ppu->t & HORIZONTAL_BITS;
            }

            if (sprite_prefetch) {
                ppu->sprite_buffer.y = ppu->sprite_eval_unit.buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
                inc_sec_oam_addr(ppu);
            }
            break;
        case NT_READ: // 1
            // load NT byte;
            ppu->bus = ppu->bus & 0xff00 | read_vram(ppu, ppu->bus);
            pu->NT = ppu->bus & 0xff;
            if (sprite_prefetch) {
                ppu->sprite_buffer.tile = ppu->sprite_eval_unit.buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
                inc_sec_oam_addr(ppu);
            }
            break;
        case AT_ADDR: // 2
            if (sprite_prefetch || pre_render)
                // load NT address
                ppu->bus = 0x2000 | ppu->v & 0xFFF;
            else
                // load AT address
                ppu->bus = 0x23C0 | ppu->v & 0x0C00 | ppu->v >> 4 & 0x38 | ppu->v >> 2 & 0x07;
            if (sprite_prefetch) {
                ppu->sprite_buffer.attr = ppu->sprite_eval_unit.buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
                inc_sec_oam_addr(ppu);
                SpriteUnit* unit = ppu->sprite_units + ((ppu->dots - 257) >> 3);
                // use unimplemented BIT 2 to mark spite zero
                unit->attr = ppu->sprite_buffer.attr | (ppu->dots == 259 && ppu->sprite_eval_unit.has_sprite_zero ? BIT_2 : 0);
            }
            break;
        case AT_READ: // 3
            // load AT/NT byte
            ppu->bus = ppu->bus & 0xff00 | read_vram(ppu, ppu->bus);
            pu->AT = ppu->bus & 0xFF;
            pu->AT >>= ppu->v >> 4 & 4 | ppu->v & 2;
            if (sprite_prefetch) {
                ppu->sprite_buffer.x = ppu->sprite_eval_unit.buffer = ppu->OAM_cache[ppu->sec_oam_address & 0x1f];
                SpriteUnit* unit = ppu->sprite_units + ((ppu->dots - 257) >> 3);
                unit->x = ppu->sprite_buffer.x;
                unit->halted = 1;
            }
            break;
        case BG_LSB_ADDR: // 4
            if (sprite_prefetch) {
                // load sprite LSB addr
                uint8_t offset = (ppu->scanlines & 0xff) - ppu->sprite_buffer.y;

                if (ppu->ctrl & LONG_SPRITE) {
                    // 8x16 sprite
                    if (ppu->sprite_buffer.attr & FLIP_VERTICAL)
                        offset ^= 15;
                    ppu->bus = (((ppu->sprite_buffer.tile & 0x1) << 12) | ((ppu->sprite_buffer.tile & ~0x1) << 4)) + ((offset & 0x8) << 1) + (offset & 0x7);
                } else {
                    // 8x8 sprite
                    if (ppu->sprite_buffer.attr & FLIP_VERTICAL)
                        offset ^= 7;
                    ppu->bus = ((ppu->ctrl & SPRITE_TABLE ? 0x1000: 0) | (ppu->sprite_buffer.tile << 4)) + (offset & 0x7);
                }
            } else {
                // load BG LSB addr
                ppu->bus = pu->NT << 4 | ppu->v >> 12 & 0x7 | (ppu->ctrl & BG_TABLE) << 8;
            }
            // address will be reused later so save it
            // the next read will corrupt lower byte
            ppu->last_addr = ppu->bus;
            break;
        case BG_LSB_READ: // 5
            ppu->bus = ppu->bus & 0xff00 | read_vram(ppu, ppu->bus);
            if (sprite_prefetch) {
                // load sprite LSB byte
                uint8_t sprite_index = (ppu->dots - 257) >> 3;
                SpriteUnit* unit = ppu->sprite_units + sprite_index;
                unit->pattern_LSB = ppu->bus & 0xff;
            }else {
                // load BG LSB byte
                pu->BG_LSB = ppu->bus & 0xff;
            }
            break;
        case BG_MSB_ADDR: // 6
            // load BG MSB / sprite MSB addr
            ppu->bus = ppu->last_addr + 8;
            break;
        case BG_MSB_READ: // 7
            ppu->bus = ppu->bus & 0xff00 | read_vram(ppu, ppu->bus);
            if (sprite_prefetch) {
                // load sprite MSB byte
                uint8_t sprite_index = (ppu->dots - 257) >> 3;
                SpriteUnit* unit = ppu->sprite_units + sprite_index;
                unit->pattern_MSB = ppu->bus & 0xff;
                inc_sec_oam_addr(ppu);

                if (!is_y_in_range(ppu, ppu->sprite_buffer.y)) {
                    // sprite is not in range
                    unit->pattern_MSB = 0;
                    unit->pattern_LSB = 0;
                }
            }else {
                // load BG MSB byte
                pu->BG_MSB = ppu->bus & 0xff;
            }

            if (ppu->dots == 256) {
                // prevent double vertical inc on $2007 read
                ppu->should_inc_vert_v = 0;
                inc_vert_v(ppu);
            }

            if (ppu->dots <= 256 || ppu->dots > 320) {
                // prevent double horizontal in on $2007 read
                ppu->should_inc_hori_v = 0;
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

    if (!(phase & 1)) {
        pu->has_set_addr = 1;
        ppu->mapper->set_bus(ppu->mapper, ppu->bus);
    } else
        pu->has_set_addr = 0;

    // glitchy increments on $2007 read
    if (ppu->should_inc_hori_v) {
        inc_hori_v(ppu);
        ppu->should_inc_hori_v = 0;
    }
    if (ppu->should_inc_vert_v) {
        inc_vert_v(ppu);
        ppu->should_inc_vert_v = 0;
    }
}

void execute_ppu(PPU* ppu) {
    // update render status
    if (ppu->render_state_delay && --ppu->render_state_delay == 0) {
        ppu->render_status = (ppu->mask & RENDER_BITS) > 0;
        if (!ppu->render_status && (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->pre_render)) {
            ppu->corrupt_oam_row = ppu->sec_oam_address & 0x1f;
            if (ppu->dots < 64 && ppu->dots <= 256)
                // round up to nearest multiple of 4
                ppu->corrupt_oam_row = (ppu->corrupt_oam_row + 3) & 0xfc;
        }
    }

    if (ppu->should_inc_v) {
        ppu->v += ((ppu->ctrl & BIT_2) ? 32 : 1);
        ppu->should_inc_v = 0;
        ppu->mapper->set_bus(ppu->mapper, ppu->v);
    }

    if (ppu->scanlines < VISIBLE_SCANLINES || ppu->scanlines == ppu->pre_render) {
        if (ppu->corrupt_oam_row && ppu->render_status) {
            ppu->corrupt_oam_row &= 0x1f;
            memcpy(ppu->OAM + (ppu->corrupt_oam_row << 3), ppu->OAM, 8);
            ppu->OAM_cache[ppu->corrupt_oam_row] = ppu->OAM_cache[0];
            ppu->corrupt_oam_row = 0;
        }
        if (ppu->dots == 0) {
            if (ppu->scanlines <= VISIBLE_SCANLINES && ppu->render_status) {
                ppu->bus = ppu->p_unit.NT << 4 | ppu->v >> 12 & 0x7 | (ppu->ctrl & BG_TABLE) << 8;
                ppu->mapper->set_bus(ppu->mapper, ppu->bus);
            }
        } else {
            // dots 1 - 256, scanline 0 - 239 (render region)
            if (ppu->scanlines < VISIBLE_SCANLINES && ppu->dots <= VISIBLE_DOTS) {
                uint16_t pixel_addr = 0x3f00;
                if (ppu->render_status) {
                    if (ppu->dots <= 64)
                        clear_oam(ppu);
                    else
                        evaluate_sprites(ppu);
                    pixel_addr = 0x3f00 | get_pixel(ppu);
                } else if ((ppu->v & 0x3fff) > 0x3f00) {
                    // if the low 14 bits of v fall within 0x3f00-0x3fff, use that value as backdrop address
                    pixel_addr = ppu->v & 0x3fff;
                }
                // only consider 6-bits from palette RAM since upper 2 bits are open bus
                pixel_addr = read_vram(ppu, pixel_addr) & 0x3f;
                // output pixel to output buffer
                ppu->screen[ppu->scanlines * 256 + ppu->dots - 1] = nes_palette[pixel_addr];
            }
            if (ppu->render_status)
                // tile and attr pre-fetch
                fetch_frame(ppu);
        }

        for (int i = 0; i < 8; i++) {
            SpriteUnit* unit = ppu->sprite_units + i;
            if (!unit->halted && unit->x) {
                if (--unit->x == 0) unit->halted = 1;
            }
        }

        if (ppu->shift_start_delay && --ppu->shift_start_delay == 0) {
            for (int i = 0; i < 8; i++) {
               ppu->sprite_units[i].halted = 0;
            }
        }

        if (ppu->render_status) {
            if (ppu->dots == 340) {
                // tell sprite shifter to go
                ppu->shift_start_delay = 1;
                // reset secondary OAM address
                ppu->sec_oam_address = 0;
            } else if (ppu->dots == 256 || ppu->dots == 64) {
                // reset secondary OAM address
                ppu->sec_oam_address = 0;
            }

        }
    }

    if (ppu->read_to_buffer) {
        switch (ppu->read_to_buffer) {
            case 3:
                // place read_address on bus if fetch unit has not already
                if (!ppu->p_unit.has_set_addr)
                    // read underlying nametable mirrors if v is pointing to palette address
                    // in that case 0x3f00 - 0x3fff maps to 0x2f00 - 0x2fff
                    ppu->bus = ppu->v >= 0x3f00 ? ppu->v & 0xefff : ppu->v;
                break;
            case 2:
                // increment v on next cycle
                inc_v(ppu);
                break;
            case 1:
                // read to buffer
                ppu->read_buffer = read_vram(ppu, ppu->bus);
                break;
            default:
                break;
        }
        ppu->read_to_buffer--;
    }

    if (ppu->write_to_buffer) {
        switch (ppu->write_to_buffer) {
            case 3:
                // place write_address on bus
                if (!ppu->p_unit.has_set_addr)
                    ppu->bus = ppu->v;
                break;
            case 2:
                // increment v on next cycle
                inc_v(ppu);
                break;
            case 1:
                // write value to VRAM
                write_vram(ppu, ppu->bus, ppu->write_val);
                ppu->bus = ppu->bus & 0xff00 | ppu->write_val;
                break;
            default:
                break;
        }
        ppu->write_to_buffer--;
    }
    ppu->p_unit.has_set_addr = 0;

    if (ppu->scanlines == 240 && ppu->dots == 0 && ppu->render_status) {
        ppu->bus = ppu->p_unit.NT << 4 | ppu->v >> 12 & 0x7 | (ppu->ctrl & BG_TABLE) << 8;
        ppu->mapper->set_bus(ppu->mapper, ppu->bus);
    }

    if(ppu->scanlines == 241 && ppu->dots == 1) {
        // set v-blank
        if (!ppu->supress_vblank)
            ppu->status |= V_BLANK;
        ppu->supress_vblank = 0;
        update_NMI(ppu, 3);
    }

    if (ppu->scanlines == ppu->pre_render) {
        // pre-render scanline 262/312
        if (ppu->dots == 0) {
            ppu->status &= ~(SPRITE_0_HIT | SPRITE_OVERFLOW);
        }
        else if(ppu->dots == 1){
            // reset v-blank and sprite zero hit
            ppu->status &= ~V_BLANK;
            update_NMI(ppu, 0);
        }
        else if(ppu->dots >= 280 && ppu->dots <= 304 && ppu->render_status){
            ppu->v &= ~VERTICAL_BITS;
            ppu->v |= ppu->t & VERTICAL_BITS;
        }

        if(ppu->dots >= 340) {
            // inform emulator to render contents of ppu on first dot
            ppu->render = 1;
            ppu->frames++;
        }
    }

    //update NMI delay
    if (ppu->nmi_delay && --ppu->nmi_delay == 0) {
        update_NMI(ppu, 0);
    }

    // open bus decay delay
    if (ppu->latch_decay && --ppu->latch_decay == 0) {
        ppu->latch = 0;
    }

    // increment dots and scanlines
    if(++ppu->dots >= DOTS_PER_SCANLINE) {
        if (ppu->scanlines++ >= ppu->pre_render) {
            // skip one cycle on odd frames if rendering is enabled for NTSC
            if (ppu->frames & 1 && ppu->render_status && ppu->emulator->type == NTSC)
                ppu->dots = 1;
            else
                ppu->dots = 0;
            ppu->scanlines = 0;
        } else {
            ppu->dots = 0;
        }
    }
}
