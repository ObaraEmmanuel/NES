#include <string.h>

#include "ppu.h"
#include "utils.h"
#include "mmu.h"
#include "cpu6502.h"

static uint8_t read_vram(PPU* ppu, uint16_t address);
static void write_vram(PPU* ppu, uint16_t address, uint8_t value);
static void render_background(PPU* ppu);
static void render_sprites(PPU* ppu);
static uint8_t* get_palette(PPU* ppu, size_t tile_x, size_t tile_y);


void init_ppu(PPU* ppu){
    memset(ppu->palette, 0, 0x20);
    memset(ppu->sprite_list, 0, 64);
    memset(ppu->V_RAM, 0, 0x800);
    memset(ppu->OAM, 0, 256);
    reset_ppu(ppu);
}

void reset_ppu(PPU* ppu){
    ppu->render = 1;
    ppu->state = PRE_RENDER;
    ppu->v = ppu->waiting_value = ppu->oam_address = ppu->t = ppu->x = ppu->cycles = ppu->scanlines = 0;
    ppu->even_frame = 1;
    ppu->ctrl &= ~0xFC;
    ppu->mask &= ~(BIT_0 | BIT_4 | BIT_3);
    ppu->mask |= (BIT_4 | BIT_3);
    ppu->status = 0;
    memset(ppu->screen, 0, sizeof(ppu->screen));
}

void set_address(PPU* ppu, uint8_t address){
    if(ppu->waiting_value){
        // second write
        ppu->t &= 0xff00;
        ppu->t |= address;
        ppu->v = ppu->t;
        ppu->waiting_value = 0;
    }else{
        // first write
        ppu->t &= 0xff;
        ppu->t |= (address & 0x3f) << 8; // store only upto bit 14
        ppu->waiting_value = 1;
    }
}


void set_oam_address(PPU* ppu, uint8_t address){
    ppu->oam_address = address % 256;
}

uint8_t read_oam(PPU* ppu){
    return ppu->OAM[ppu->oam_address];
}

void write_oam(PPU* ppu, uint8_t value){
    ppu->OAM[ppu->oam_address++] = value;
}

void set_scroll(PPU* ppu, uint8_t coord){
    if(ppu->waiting_value){
        // second write
        ppu->t &= ~Y_SCROLL_BITS;
        ppu->t |= ((coord & 0x7) << 12) | ((coord & 0xF8) << 2);
        ppu->waiting_value = 0;
    }else{
        // first write
        ppu->t &= ~X_SCROLL_BITS;
        ppu->t |= (coord >> 3) & X_SCROLL_BITS;
        ppu->x = coord & 0x7;
        ppu->waiting_value = 1;
    }
}

uint8_t read_ppu(PPU* ppu){
    uint8_t data = read_vram(ppu, ppu->v);
    if(ppu->v < 0x3F00){
        // reads in this range are read from buffer and the current value buffered
        uint8_t temp = ppu->buffer;
        ppu->buffer = data;
        data = temp;
    }
    ppu->v += ((ppu->ctrl & BIT_2) ? 32 : 1);
    return data;
}

void write_ppu(PPU* ppu, uint8_t value){
    write_vram(ppu, ppu->v, value);
    ppu->v += ((ppu->ctrl & BIT_2) ? 32 : 1);
}

void dma(PPU* ppu, struct Memory* memory, uint8_t address){
    uint8_t* ptr = get_ptr(memory, address * 0x100);
    // copy from OAM address to the end (256 bytes)
    memcpy(ppu->OAM + ppu->oam_address, ptr, 256 - ppu->oam_address);
    if(ppu->oam_address)
        // wrap around and copy from start to OAM address if OAM is not 0x00
        memcpy(ppu->OAM, ptr + (256 - ppu->oam_address), ppu->oam_address);
    memory->cpu->cycles += 513;
    // TODO : Handle oddity
}



static uint8_t read_vram(PPU* ppu, uint16_t address){
    address = address % 0x4000;

    if(address < 0x2000)
        return ppu->mapper->read_CHR(ppu->mapper, address);

    if(address < 0x3F00){
        address = (address - 0x2000) % 0x1000;
        if(address < 0x400)
            return ppu->V_RAM[ppu->mapper->name_table_map[0] + address % 0x400];
        else if(address < 0x800)
            return ppu->V_RAM[ppu->mapper->name_table_map[1] + address % 0x400];
        else if(address < 0xC00)
            return ppu->V_RAM[ppu->mapper->name_table_map[2] + address % 0x400];
        else
            return ppu->V_RAM[ppu->mapper->name_table_map[3] + address % 0x400];
    }

    if(address < 0x4000)
        return ppu->palette[(address - 0x3F00) % 0x20];

    return 0;
}

static void write_vram(PPU* ppu, uint16_t address, uint8_t value){
    address = address % 0x4000;

    if(address < 0x2000)
        ppu->mapper->write_CHR(ppu->mapper, address, value);
    else if(address < 0x3F00){
        address = (address - 0x2000) % 0x1000;
        if(address < 0x400)
            ppu->V_RAM[ppu->mapper->name_table_map[0] + address % 0x400] = value;
        else if(address < 0x800)
            ppu->V_RAM[ppu->mapper->name_table_map[1] + address % 0x400] = value;
        else if(address < 0xC00)
            ppu->V_RAM[ppu->mapper->name_table_map[2] + address % 0x400] = value;
        else
            ppu->V_RAM[ppu->mapper->name_table_map[3] + address % 0x400] = value;
    }

    else if(address < 0x4000) {
        address = (address - 0x3F00) % 0x20;
        if(address == 0x10)
            ppu->palette[0] = value;
        else
            ppu->palette[address] = value;
    }

}

uint8_t read_status(PPU* ppu){
    uint8_t status = ppu->status;
    ppu->waiting_value = 0;
    ppu->status &= ~BIT_7; // reset v_blank
    return status;
}

void set_ctrl(PPU* ppu, uint8_t ctrl){
    ppu->ctrl = ctrl;
    // set name table in temp address
    ppu->t &= ~0xc00;
    ppu->t |= (ctrl & (BIT_0 | BIT_1)) << 10;
}

void execute_ppu(PPU* ppu){
    if(ppu->scanlines < VISIBLE_SCAN_LINES && ppu->cycles < SCANLINE_VISIBLE_DOTS){
        // update sprite zero hit
        ppu->status &= ~BIT_6;
        ppu->status |= ppu->OAM[0] == ppu->scanlines && ppu->cycles == ppu->OAM[3] && ppu->mask & BIT_4 ? BIT_6 : 0;
    }
    ppu->cycles++;
    if(ppu->cycles >= 341){
        ppu->cycles = 0;
        ppu->scanlines++;
        if(ppu->scanlines == 240){
            render_background(ppu);
            render_sprites(ppu);
        }
        else if(ppu->scanlines == 241){
            // set v-blank
            ppu->status |= BIT_7;
            if(ppu->ctrl & BIT_7){
                // generate NMI
                interrupt(ppu->mem->cpu, NMI);
            }
        }
        else if(ppu->scanlines >= 262){
            ppu->scanlines = 0;
            // reset v-blank and sprite zero hit
            ppu->status &= ~(BIT_7 | BIT_6);
            ppu->render = 1;
        }
    }
}

static void render_background(PPU* ppu) {
    uint16_t bank = ppu->ctrl & (BIT_4) ? 0x1000 : 0;
    for(int i = 0; i < 0x3C0; i++){
        size_t tile_x = i % 32;
        size_t tile_y = i / 32;
        uint8_t* tile = ppu->mapper->CHR_RAM + bank + ppu->V_RAM[i] * 16;
        uint8_t* palette = get_palette(ppu, tile_x, tile_y);

        for(int y = 0; y < 8; y++){
            uint8_t lo = *(tile + y);
            uint8_t hi = *(tile + y + 8);

            for(int x = 7; x >= 0; x--){
                uint8_t value = ((hi & BIT_0) << 1) | lo & BIT_0;
                uint32_t color = value == 0? nes_palette[ppu->palette[0]] : nes_palette[*(palette + value)];
                hi >>= 1;
                lo >>= 1;
                ppu->screen[(y + tile_y * 8) * SCANLINE_VISIBLE_DOTS + (x + tile_x *8)] = color;
            }
        }
    }
}

static void render_sprites(PPU* ppu){
    // 4 bytes per sprite
    // byte 0 -> y index
    // byte 1 -> tile index
    // byte 2 -> render info
    // byte 3 -> x index
    uint16_t bank = ppu->ctrl & (BIT_3) ? 0x1000 : 0;

    for(int i = 0x100 - 4; i >= 0; i-=4){
        uint16_t tile_index = ppu->OAM[i + 1];
        uint8_t tile_x = ppu->OAM[i + 3];
        uint8_t tile_y = ppu->OAM[i];
        uint8_t attr = ppu->OAM[i + 2];
        uint8_t palette_index = attr & 0x3;
        uint8_t* palette = ppu->palette + 0x10 + palette_index * 4;
        uint8_t* tile = ppu->mapper->CHR_RAM + bank + tile_index * 16;
        for(int y = 0; y < 8; y++){
            uint8_t lo = *(tile + y);
            uint8_t hi = *(tile + y + 8);

            for(int x = 7; x >= 0; x--){
                uint8_t value = ((hi & BIT_0) << 1) | lo & BIT_0;
                hi >>= 1;
                lo >>= 1;
                if(!value)
                    continue;
                uint32_t color = nes_palette[*(palette + value)];
                size_t screen_index =  ((attr & BIT_7 ? 7 - y : y) + tile_y) * SCANLINE_VISIBLE_DOTS + ((attr & BIT_6 ? 7 - x : x) + tile_x);
                // clip
                if(screen_index < SCANLINE_VISIBLE_DOTS * VISIBLE_SCAN_LINES)
                    ppu->screen[screen_index] = color;
            }
        }

    }
}

static uint8_t* get_palette(PPU* ppu, size_t tile_x, size_t tile_y){
    uint8_t index = (tile_y / 4) * 8 + tile_x / 4;
    uint8_t attr = ppu->V_RAM[0x3C0 + index];

    uint8_t palette_index = (attr >> ((((tile_y % 4) / 2) << 1) | ((tile_x % 4) / 2)) * 2) & 0x3 ;
    return ppu->palette + palette_index * 4;
}
