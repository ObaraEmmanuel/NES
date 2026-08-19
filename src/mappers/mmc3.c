#include <emulator.h>
#include <stdint.h>
#include <stdlib.h>

#include "mapper.h"
#include "utils.h"
#include "cpu6502.h"

typedef struct {
    uint8_t *PRG_bank_ptrs[4];
    uint8_t *CHR_bank_ptrs[8];
    uint8_t PRG_mode;
    uint8_t CHR_inversion;
    uint8_t next_bank_data;
    uint8_t a12;
    uint8_t IRQ_latch;
    uint8_t IRQ_counter;
    uint8_t IRQ_cleared;
    uint8_t IRQ_enabled;
    uint8_t alt_clocking;
    uint8_t RAM_enabled;
    uint8_t RAM_protect;
    uint32_t PRG_clamp;
    uint32_t CHR_clamp;
    size_t cycles;
} MMC3_t;

static uint8_t read_ROM_MMC6(Mapper *mapper, uint16_t address);

static void write_ROM_MMC6(Mapper *mapper, uint16_t address, uint8_t value);

static uint8_t read_PRG(Mapper * mapper, uint16_t addr);

static void write_PRG(Mapper * mapper, uint16_t addr, uint8_t val);

static uint8_t read_CHR(Mapper * mapper, uint16_t addr);

static void write_bank_data(Mapper *mapper, uint8_t val);

static void on_filtered_a12(Mapper* mapper);

static void set_bus(Mapper * mapper, uint16_t addr);

static void set_bus_mc_acc(Mapper * mapper, uint16_t addr);


int load_MMC3(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->set_bus = set_bus;
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

    if (mapper->submapper == 1) {
        // MMC6: enable full write protection
        mapper->write_ROM = write_ROM_MMC6;
        mapper->read_ROM  = read_ROM_MMC6;
    } else if (mapper->submapper == 3) {
        // use MC-ACC clocking behaviour
        mapper->set_bus = set_bus_mc_acc;
    } else if (mapper->submapper == 4) {
        // NEC uses alternate clocking
        mmc3->alt_clocking = 1;
    } else if (mapper->submapper == 5) {
        LOG(WARN, "T9552 de-scrambling required");
    }
    return 0;
}

static void set_bus(Mapper *mapper, uint16_t addr) {
    MMC3_t *mmc3 = mapper->extension;
    uint8_t a12 = (addr & 0x1000) > 0;
    if (a12 > mmc3->a12) {
        if (mapper->emulator->cpu.t_cycles - mmc3->cycles >= 3) {
            // clock counter
            on_filtered_a12(mapper);
        }
    }
    if (a12) mmc3->cycles = mapper->emulator->cpu.t_cycles;
    mmc3->a12 = a12;
}

static void set_bus_mc_acc(Mapper *mapper, uint16_t addr) {
    MMC3_t *mmc3 = mapper->extension;
    uint8_t a12 = (addr & 0x1000) > 0;
    if (mmc3->a12 > a12) {
        mmc3->cycles++;
        if (mmc3->cycles == 1) {
            // clock counter
            on_filtered_a12(mapper);
        } else if (mmc3->cycles == 8) {
            mmc3->cycles = 0;
        }
    }
    mmc3->a12 = a12;
}

static void on_filtered_a12(Mapper* mapper) {
    MMC3_t *mmc3 = mapper->extension;
    uint8_t counter = mmc3->IRQ_counter;
    if (mmc3->IRQ_cleared || !mmc3->IRQ_counter)
        mmc3->IRQ_counter = mmc3->IRQ_latch;
    else
        mmc3->IRQ_counter--;

    if (mmc3->alt_clocking) {
        if ((counter > 0 || mmc3->IRQ_cleared) && !mmc3->IRQ_counter && mmc3->IRQ_enabled)
            interrupt(&mapper->emulator->cpu, MAPPER_IRQ);
    } else if (!mmc3->IRQ_counter && mmc3->IRQ_enabled)
        interrupt(&mapper->emulator->cpu, MAPPER_IRQ);

    mmc3->IRQ_cleared = 0;
}

static uint8_t read_ROM_MMC6(Mapper *mapper, uint16_t address) {
    if (address < 0x6000) {
        // expansion rom
        LOG(DEBUG, "Attempted to read from unavailable expansion ROM");
        return mapper->emulator->mem.bus;
    }
    if (address < 0x8000) {
        // PRG ram
        if (mapper->PRG_RAM == NULL) {
            LOG(DEBUG, "Attempted to read from non existent PRG RAM");
            return mapper->emulator->mem.bus;
        }
        MMC3_t *mmc3 = mapper->extension;
        // no bank is enabled so 0x7000 - 0x7fff is open bus
        if ((mmc3->RAM_protect & 0xA0) == 0)
            return mapper->emulator->mem.bus;

        // at least one bank is enabled, so disabled bank returns 0 and not open bus
        uint16_t ram_addr = address - 0x6000 & mapper->PRG_RAM_clamp;
        if (ram_addr < 0x7200) {
            if (mmc3->RAM_protect & BIT_5)
                return mapper->PRG_RAM[ram_addr];
            return 0;
        }

        if (mmc3->RAM_protect & BIT_7)
            return mapper->PRG_RAM[ram_addr];
        return 0;
    }

    // PRG
    return mapper->read_PRG(mapper, address);
}

static void write_ROM_MMC6(Mapper *mapper, uint16_t address, uint8_t value) {
    if (address < 0x6000) {
        LOG(DEBUG, "Attempted to write to unavailable expansion ROM");
        return;
    }

    if (address < 0x8000) {
        // extended ram
        if (mapper->PRG_RAM == NULL) {
            LOG(DEBUG, "Attempted to write to non existent PRG RAM");
            return;
        }
        MMC3_t *mmc3 = mapper->extension;
        uint16_t ram_addr = address - 0x6000 & mapper->PRG_RAM_clamp;
        // if writing is enabled but reading is not for that bank,
        // then the writing can't go through
        if (ram_addr < 0x7200) {
            if ((mmc3->RAM_protect & 0x30) == 0x30)
                mapper->PRG_RAM[ram_addr] = value;
        } else if ((mmc3->RAM_protect & 0xC0) == 0xC0) {
            mapper->PRG_RAM[ram_addr] = value;
        }
        return;
    }

    // PRG
    mapper->write_PRG(mapper, address, value);
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
            mmc3->RAM_enabled = (val >> 5) & 1;
            if (!mmc3->RAM_enabled)
                mmc3->RAM_protect = 0;
            break;
        case 0x8001:
            write_bank_data(mapper, val);
            break;
        case 0xA000:
            if (mapper->mirroring != FOUR_SCREEN)
                set_mirroring(mapper, val & 1 ? HORIZONTAL : VERTICAL);
            break;
        case 0xA001:
            if (mmc3->RAM_enabled)
                mmc3->RAM_protect = val;
            break;
        case 0xC000:
            mmc3->IRQ_latch = val;
            if (mapper->submapper == 4 && !mmc3->IRQ_latch)
                mmc3->IRQ_enabled = 0;
            break;
        case 0xC001:
            mmc3->IRQ_cleared = 1;
            if (mapper->submapper == 3) {
                // MC-ACC resets pulse counter on $C001 write
                mmc3->cycles = 0;
            }
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
