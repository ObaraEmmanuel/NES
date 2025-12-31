#include <emulator.h>
#include <stdint.h>

#include "mapper.h"
#include "utils.h"
#include "cpu6502.h"

typedef struct {
    uint8_t *PRG_bank_ptrs[4];
    uint8_t *CHR_bank_ptrs[8];
    uint8_t PRG_mode;
    uint8_t CHR_inversion;
    uint8_t next_bank_data;
    uint8_t IRQ_latch;
    uint8_t IRQ_counter;
    uint8_t IRQ_cleared;
    uint8_t IRQ_enabled;
    uint32_t PRG_clamp;
    uint32_t CHR_clamp;
} MMC3_t;


static uint8_t read_PRG(Mapper * mapper, uint16_t addr);

static void write_PRG(Mapper * mapper, uint16_t addr, uint8_t val);

static uint8_t read_CHR(Mapper * mapper, uint16_t addr);

static void write_bank_data(Mapper *mapper, uint8_t val);

static void on_scanline(Mapper* mapper);


int load_MMC3(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->on_scanline = on_scanline;
    MMC3_t *mmc3 = calloc(1, sizeof(MMC3_t));
    mapper->extension = mmc3;
    // PRG banks in 8k chunks
    mmc3->PRG_clamp = next_power_of_2(mapper->PRG_banks * 2);
    mmc3->PRG_clamp = mmc3->PRG_clamp > 0 ? mmc3->PRG_clamp - 1: 0;
    // CHR banks in 1k chunks
    mmc3->CHR_clamp = next_power_of_2(mapper->CHR_banks * 8);
    mmc3->CHR_clamp = mmc3->CHR_clamp > 0 ? mmc3->CHR_clamp - 1: 0;

    // last bank
    mmc3->PRG_bank_ptrs[3] = mapper->PRG_ROM + (mapper->PRG_banks) * 0x4000 - 0x2000;
    // 2nd last bank
    mmc3->PRG_bank_ptrs[2] = mmc3->PRG_bank_ptrs[3] - 0x2000;

    // point to first banks to prevent seg faults just in case
    mmc3->PRG_bank_ptrs[0] = mmc3->PRG_bank_ptrs[1] = mapper->PRG_ROM;

    // point CHR to first bank
    for(int i = 0; i < 8; i++)
        mmc3->CHR_bank_ptrs[i] = mapper->CHR_ROM;
    mmc3->CHR_bank_ptrs[1] = mmc3->CHR_bank_ptrs[0] + 0x400;
    mmc3->CHR_bank_ptrs[3] = mmc3->CHR_bank_ptrs[0] + 0x400;
    return 0;
}

static void on_scanline(Mapper* mapper) {
    // TODO cycle-accurate A12 based IRQ
    MMC3_t *mmc3 = mapper->extension;

    if(mmc3->IRQ_cleared || !mmc3->IRQ_counter) {
        mmc3->IRQ_counter = mmc3->IRQ_latch;
        mmc3->IRQ_cleared = 0;
    }else {
        mmc3->IRQ_counter--;
    }

    if(!mmc3->IRQ_counter && mmc3->IRQ_enabled)
        interrupt(&mapper->emulator->cpu, MAPPER_IRQ);
}

uint8_t read_PRG(Mapper *mapper, uint16_t addr) {
    MMC3_t *mmc3 = mapper->extension;
    switch (addr & 0xE000) {
        case 0x8000:
            if (mmc3->PRG_mode) {
                // 2nd last
                return *(mmc3->PRG_bank_ptrs[2] + (addr - 0x8000));
            }
            // R6
            return *(mmc3->PRG_bank_ptrs[0] + (addr - 0x8000));
        case 0xA000:
            // R7
            return *(mmc3->PRG_bank_ptrs[1] + (addr - 0xA000));
        case 0xC000:
            if (mmc3->PRG_mode) {
                // R6
                return *(mmc3->PRG_bank_ptrs[0] + (addr - 0xC000));
            }
            // 2nd-last
            return *(mmc3->PRG_bank_ptrs[2] + (addr - 0xC000));
        case 0xE000:
            // last
            return *(mmc3->PRG_bank_ptrs[3] + (addr - 0xE000));
        default:
            // out of bounds
            LOG(ERROR, "PRG Read (0x%04x) out of bounds", addr);
            return 0;
    }
}

void write_PRG(Mapper *mapper, uint16_t addr, uint8_t val) {
    MMC3_t *mmc3 = mapper->extension;
    switch (addr & 0xE001) {
        case 0x8000:
            mmc3->next_bank_data = val & 0x7;
            mmc3->PRG_mode = (val >> 6) & 1;
            mmc3->CHR_inversion = (val >> 7) & 1;
            break;
        case 0x8001:
            write_bank_data(mapper, val);
            break;
        case 0xA000:
            if (mapper->mirroring != FOUR_SCREEN)
                set_mirroring(mapper, val & 1 ? HORIZONTAL : VERTICAL);
            break;
        case 0xA001:
            // PRG RAM write protect stuff
            break;
        case 0xC000:
            mmc3->IRQ_latch = val;
            break;
        case 0xC001:
            mmc3->IRQ_counter = 0;
            mmc3->IRQ_cleared = 1;
            break;
        case 0xE000:
            mmc3->IRQ_enabled = 0;
            // acknowledge pending interrupts
            interrupt_clear(&mapper->emulator->cpu, MAPPER_IRQ);
            break;
        case 0xE001:
            mmc3->IRQ_enabled = 1;
            break;
        default:
            // out of bounds
            LOG(ERROR, "PRG Write (0x%04x) out of bounds", addr);
            break;
    }
}

void write_bank_data(Mapper *mapper, uint8_t val) {
    MMC3_t *mmc3 = mapper->extension;
    switch (mmc3->next_bank_data) {
        case 0:
            // R0
            val = val & 0xFE;
            val &= mmc3->CHR_clamp;
            mmc3->CHR_bank_ptrs[0] = mapper->CHR_ROM + val * 0x400;
            mmc3->CHR_bank_ptrs[1] = mmc3->CHR_bank_ptrs[0] + 0x400;
            break;
        case 1:
            // R1
            val = val & 0xFE;
            val &= mmc3->CHR_clamp;
            mmc3->CHR_bank_ptrs[2] = mapper->CHR_ROM + val * 0x400;
            mmc3->CHR_bank_ptrs[3] = mmc3->CHR_bank_ptrs[2] + 0x400;
            break;
        case 2:case 3:case 4:case 5:
            // R2/R3/R4/R5
            val &= mmc3->CHR_clamp;
            mmc3->CHR_bank_ptrs[mmc3->next_bank_data + 2] = mapper->CHR_ROM + val * 0x400;
            break;
        case 6:case 7:default:
            // R6/R7
            val &= mmc3->PRG_clamp;
            mmc3->PRG_bank_ptrs[mmc3->next_bank_data - 6] = mapper->PRG_ROM + val * 0x2000;
            break;
    }
}

uint8_t read_CHR(Mapper *mapper, uint16_t addr) {
    if(!mapper->CHR_banks) {
        return mapper->CHR_ROM[addr];
    }
    MMC3_t *mmc3 = mapper->extension;
    uint8_t ptr_index = (addr & 0x1C00) / 0x400;
    addr = addr - (addr & 0x1C00);
    if(mmc3->CHR_inversion) {
        ptr_index = (ptr_index + 4) % 8;
    }
    return mmc3->CHR_bank_ptrs[ptr_index][addr];
}
