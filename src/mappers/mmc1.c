#include <emulator.h>
#include <stdlib.h>

#include "mapper.h"
#include "utils.h"

typedef struct {
    uint8_t PRG_reg;
    uint8_t CHR1_reg;
    uint8_t CHR2_reg;
    uint8_t *PRG_bank1;
    uint8_t *PRG_bank2;
    uint8_t *CHR_bank1;
    uint8_t *CHR_bank2;
    uint8_t CHR_mode;
    uint8_t PRG_mode;
    uint8_t reg;
    uint32_t PRG_clamp;
    uint32_t CHR_clamp;
    size_t cpu_cycle;
} MMC1_t;

enum {
    MIRROR_BITS = 0x3,
    REG_RESET = BIT_7,
    CHR_MODE = BIT_4,
    PRG_MODE = 0xC,
    REG_INIT = 0b100000
};

static uint8_t read_PRG(Mapper *mapper, uint16_t address);
static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper *mapper, uint16_t address);
static void set_PRG_banks(MMC1_t *mmc1, Mapper *mapper);
static void set_CHR_banks(MMC1_t *mmc1, Mapper *mapper);

int load_MMC1(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    MMC1_t *mmc1 = calloc(1, sizeof(MMC1_t));
    mapper->extension = mmc1;
    mmc1->reg = REG_INIT;
    mmc1->PRG_mode = 3;
    mmc1->cpu_cycle = -1;
    // PRG banks in 8k chunks
    mmc1->PRG_clamp = next_power_of_2(mapper->PRG_banks);
    mmc1->PRG_clamp = mmc1->PRG_clamp > 0 ? mmc1->PRG_clamp - 1 : 0;
    // CHR banks in 1k chunks
    mmc1->CHR_clamp = next_power_of_2(mapper->CHR_banks * 2);
    mmc1->CHR_clamp = mmc1->CHR_clamp > 0 ? mmc1->CHR_clamp - 1 : 0;

    if (mapper->CHR_banks) {
        mmc1->CHR_bank1 = mapper->CHR_ROM;
        mmc1->CHR_bank2 = mmc1->CHR_bank1 + 0x1000;
    }

    mmc1->PRG_bank1 = mapper->PRG_ROM;
    mmc1->PRG_bank2 = mapper->PRG_ROM + (mapper->PRG_banks - 1) * 0x4000;
    return 0;
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    if (address < 0xC000)
        return *(((MMC1_t *) mapper->extension)->PRG_bank1 + (address & 0x3fff));
    return *(((MMC1_t *) mapper->extension)->PRG_bank2 + (address & 0x3fff));
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    MMC1_t *mmc1 = mapper->extension;
    uint8_t same_cycle = mapper->emulator->cpu.t_cycles == mmc1->cpu_cycle;
    mmc1->cpu_cycle = mapper->emulator->cpu.t_cycles;
    if (value & REG_RESET) {
        // reset
        mmc1->reg = REG_INIT;
        mmc1->PRG_mode = 3;
        set_PRG_banks(mmc1, mapper);
    } else {
        // ignore consequtive writes
        if (same_cycle)
            return;
        mmc1->reg = (mmc1->reg >> 1) | ((value & BIT_0) << 5);

        if (!(mmc1->reg & BIT_0))
            // register not yet full
            return;

        // remove register size check bit
        mmc1->reg >>= 1;
        switch (address & 0xE000) {
            case 0x8000:
                // mirroring
                switch (mmc1->reg & MIRROR_BITS) {
                    case 0: set_mirroring(mapper, ONE_SCREEN_LOWER);
                        break;
                    case 1: set_mirroring(mapper, ONE_SCREEN_UPPER);
                        break;
                    case 2: set_mirroring(mapper, VERTICAL);
                        break;
                    case 3: set_mirroring(mapper, HORIZONTAL);
                        break;
                    default: break;
                }

                mmc1->CHR_mode = (mmc1->reg & CHR_MODE) >> 4;
                mmc1->PRG_mode = (mmc1->reg & PRG_MODE) >> 2;

                set_PRG_banks(mmc1, mapper);
                set_CHR_banks(mmc1, mapper);
                break;
            case 0xa000:
                if (mapper->CHR_RAM_size) {
                    mmc1->PRG_reg &= ~BIT_4;
                    mmc1->PRG_reg |= mmc1->reg & BIT_4;
                    mmc1->PRG_reg &= mmc1->PRG_clamp;
                    set_PRG_banks(mmc1, mapper);
                } else {
                    mmc1->CHR1_reg = mmc1->reg & 0x1f;
                    mmc1->CHR1_reg &= mmc1->CHR_clamp;
                    set_CHR_banks(mmc1, mapper);
                }

                break;
            case 0xc000:
                if (!mmc1->CHR_mode)
                    break;
                if (mapper->CHR_RAM_size) {
                    mmc1->PRG_reg &= ~BIT_4;
                    mmc1->PRG_reg |= mmc1->reg & BIT_4;
                    mmc1->PRG_reg &= mmc1->PRG_clamp;
                    set_PRG_banks(mmc1, mapper);
                } else {
                    mmc1->CHR2_reg = mmc1->reg & 0x1f;
                    mmc1->CHR2_reg &= mmc1->CHR_clamp;
                    set_CHR_banks(mmc1, mapper);
                }
                break;
            case 0xe000:
                mmc1->PRG_reg &= ~0xf;
                mmc1->PRG_reg |= mmc1->reg & 0xF;
                mmc1->PRG_reg &= mmc1->PRG_clamp;
                set_PRG_banks(mmc1, mapper);
                break;
            default:
                break;
        }
        mmc1->reg = REG_INIT;
    }
}

static void set_PRG_banks(MMC1_t *mmc1, Mapper *mapper) {
    switch (mmc1->PRG_mode) {
        case 0:
        case 1:
            mmc1->PRG_bank1 = mapper->PRG_ROM + (0x4000 * (mmc1->PRG_reg & ~1));
            mmc1->PRG_bank2 = mmc1->PRG_bank1 + 0x4000;
            break;
        case 2:
            // fix first bank switch second bank
            mmc1->PRG_bank1 = mapper->PRG_ROM + (0x4000 * (mmc1->PRG_reg & BIT_4));
            mmc1->PRG_bank2 = mapper->PRG_ROM + 0x4000 * mmc1->PRG_reg;
            break;
        case 3:
            // fix second bank switch first bank
            mmc1->PRG_bank1 = mapper->PRG_ROM + 0x4000 * mmc1->PRG_reg;
            if (mapper->PRG_banks > 16)
                mmc1->PRG_bank2 = mapper->PRG_ROM + (((mmc1->PRG_reg & BIT_4) > 0) + 1) * 0x40000 - 0x4000;
            else
                mmc1->PRG_bank2 = mapper->PRG_ROM + (mapper->PRG_banks - 1) * 0x4000;
            break;
        default:
            break;
    }
}

static void set_CHR_banks(MMC1_t *mmc1, Mapper *mapper) {
    if (mmc1->CHR_mode) {
        // 2 4KB banks
        mmc1->CHR_bank1 = mapper->CHR_ROM + (0x1000 * mmc1->CHR1_reg);
        mmc1->CHR_bank2 = mapper->CHR_ROM + (0x1000 * mmc1->CHR2_reg);
    } else {
        mmc1->CHR_bank1 = mapper->CHR_ROM + (0x1000 * (mmc1->CHR1_reg & ~1));
        mmc1->CHR_bank2 = mmc1->CHR_bank1 + 0x1000;
    }
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    if (mapper->CHR_RAM_size)
        return mapper->CHR_ROM[address];
    if (address < 0x1000)
        return *(((MMC1_t *) mapper->extension)->CHR_bank1 + address);
    return *(((MMC1_t *) mapper->extension)->CHR_bank2 + (address & 0xfff));
}
