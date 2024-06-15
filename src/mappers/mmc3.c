#include <mapper.h>
#include <stdint.h>
#include <utils.h>

typedef struct {
    uint8_t *CHR_bank_2k_1;
    uint8_t *CHR_bank_2k_2;
    uint8_t *CHR_bank_1k_1;
    uint8_t *CHR_bank_1k_2;
    uint8_t *CHR_bank_1k_3;
    uint8_t *CHR_bank_1k_4;
    uint8_t *PRG_bank1;
    uint8_t *PRG_bank2;
    uint8_t *PRG_bank3;
    uint8_t *PRG_bank4;
    uint8_t PRG_mode;
    uint8_t CHR_inversion;
    uint8_t next_bank_data;
    uint8_t IRQ_latch;
    uint8_t IRQ_counter;
    uint8_t IRQ_cleared;
    uint8_t IRQ_enabled;
} MMC3_t;


static uint8_t read_PRG(Mapper *, uint16_t);

static void write_PRG(Mapper *, uint16_t, uint8_t);

static uint8_t read_CHR(Mapper *, uint16_t);

static void write_CHR(Mapper *, uint16_t, uint8_t);

static void write_bank_data(Mapper *mapper, uint8_t val);


void load_MMC3(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    MMC3_t *mmc3 = calloc(1, sizeof(MMC3_t));
    mapper->extension = mmc3;

    // last bank
    mmc3->PRG_bank4 = mapper->PRG_ROM + (mapper->PRG_banks) * 0x4000 - 0x2000;
    // 2nd last bank
    mmc3->PRG_bank3 = mmc3->PRG_bank4 - 0x2000;

    // point to first banks to prevent seg faults just in case
    mmc3->PRG_bank1 = mmc3->PRG_bank2 = mapper->PRG_ROM;

    // point to first bank
    mmc3->CHR_bank_1k_1 = mmc3->CHR_bank_1k_2 = mmc3->CHR_bank_1k_2 = mmc3->CHR_bank_1k_2 = mapper->CHR_ROM;
    mmc3->CHR_bank_2k_1 = mmc3->CHR_bank_2k_2 = mapper->CHR_ROM;
}

uint8_t read_PRG(Mapper *mapper, uint16_t addr) {
    MMC3_t *mmc3 = mapper->extension;
    switch (addr & 0xE000) {
        case 0x8000:
            if (mmc3->PRG_mode) {
                // 2nd last
                return *(mmc3->PRG_bank3 + (addr - 0x8000));
            }
            // R6
            return *(mmc3->PRG_bank1 + (addr - 0x8000));
        case 0xA000:
            // R7
            return *(mmc3->PRG_bank2 + (addr - 0xA000));
        case 0xC000:
            if (mmc3->PRG_mode) {
                // R6
                return *(mmc3->PRG_bank1 + (addr - 0xC000));
            }
            // 2nd-last
            return *(mmc3->PRG_bank3 + (addr - 0xC000));
        case 0xE000:
            // last
            return *(mmc3->PRG_bank4 + (addr - 0xE000));
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
            // ack pending interrupts?
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
            mmc3->CHR_bank_2k_1 = mapper->CHR_ROM + val * 0x400;
            break;
        case 1:
            // R1
            val = val & 0xFE;
            mmc3->CHR_bank_2k_2 = mapper->CHR_ROM + val * 0x400;
            break;
        case 2:
            // R2
            mmc3->CHR_bank_1k_1 = mapper->CHR_ROM + val * 0x400;
            break;
        case 3:
            // R3
            mmc3->CHR_bank_1k_2 = mapper->CHR_ROM + val * 0x400;
            break;
        case 4:
            // R4
            mmc3->CHR_bank_1k_3 = mapper->CHR_ROM + val * 0x400;
            break;
        case 5:
            // R5
            mmc3->CHR_bank_1k_4 = mapper->CHR_ROM + val * 0x400;
            break;
        case 6:
            // R6
            val = val & 0x3f;
            mmc3->PRG_bank1 = mapper->PRG_ROM + val * 0x2000;
            break;
        case 7:
        default:
            // R7
            val = val & 0x3f;
            mmc3->PRG_bank1 = mapper->PRG_ROM + val * 0x2000;
            break;
    }
}

uint8_t read_CHR(Mapper *mapper, uint16_t addr) {
    MMC3_t *mmc3 = mapper->extension;
    switch (addr & 0x1C00) {
        case 0x0:
            // R2 / R0
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_1k_1 + addr) : *(mmc3->CHR_bank_2k_1 + addr);
        case 0x400:
            // R3 / R0
            addr = addr - 0x400;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_1k_2 + addr) : *(mmc3->CHR_bank_2k_1 + 0x400 + addr);
        case 0x800:
            // R4 / R1
            addr = addr - 0x800;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_1k_3 + addr) : *(mmc3->CHR_bank_2k_2 + addr);
        case 0xC00:
            // R5 / R1
            addr = addr - 0xC00;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_1k_4 + addr) : *(mmc3->CHR_bank_2k_2 + 0x400 + addr);
        case 0x1000:
            // R0 / R2
            addr = addr - 0x1000;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_2k_1 + addr) : *(mmc3->CHR_bank_1k_1 + addr);
        case 0x1400:
            // R0 / R3
            addr = addr - 0x1400;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_2k_1 + 0x400 + addr) : *(mmc3->CHR_bank_1k_2 + addr);
        case 0x1800:
            // R1 / R4
            addr = addr - 0x1800;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_2k_2 + addr) : *(mmc3->CHR_bank_1k_3 + addr);
        case 0x1C00:
            // R1 / R5
            addr = addr - 0x1C00;
            return mmc3->CHR_inversion ? *(mmc3->CHR_bank_2k_2 + 0x400 + addr) : *(mmc3->CHR_bank_1k_4 + addr);
        default:
            // out of bounds
            LOG(ERROR, "CHR Read (0x%04x) out of bounds", addr);
            return 0;
    }
}

void write_CHR(Mapper *mapper, uint16_t addr, uint8_t val) {
    MMC3_t *mmc3 = mapper->extension;
    switch (addr & 0x1C00) {
        case 0x0:
            // R2 / R0
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_1k_1 + addr) = val;
            else
                *(mmc3->CHR_bank_2k_1 + addr) = val;
            break;
        case 0x400:
            // R3 / R0
            addr = addr - 0x400;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_1k_2 + 0x400 + addr) = val;
            else
                *(mmc3->CHR_bank_2k_1 + addr) = val;
            break;
        case 0x800:
            // R4 / R1
            addr = addr - 0x800;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_1k_3 + addr) = val;
            else
                *(mmc3->CHR_bank_2k_2 + addr) = val;
            break;
        case 0xC00:
            // R5 / R1
            addr = addr - 0xC00;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_1k_4 + addr) = val;
            else
                *(mmc3->CHR_bank_2k_2 + 0x400 + addr) = val;
            break;
        case 0x1000:
            // R0 / R2
            addr = addr - 0x1000;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_2k_1 + addr) = val;
            else
                *(mmc3->CHR_bank_1k_1 + addr) = val;
            break;
        case 0x1400:
            // R0 / R3
            addr = addr - 0x1400;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_2k_1 + 0x400 + addr) = val;
            else
                *(mmc3->CHR_bank_1k_2 + addr) = val;
            break;
        case 0x1800:
            // R1 / R4
            addr = addr - 0x1800;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_2k_2 + addr) = val;
            else
                *(mmc3->CHR_bank_1k_3 + addr) = val;
            break;
        case 0x1C00:
            // R1 / R5
            addr = addr - 0x1C00;
            if (mmc3->CHR_inversion)
                *(mmc3->CHR_bank_2k_2 + 0x400 + addr) = val;
            else
                *(mmc3->CHR_bank_1k_4 + addr) = val;
            break;
        default:
            // out of bounds
            LOG(ERROR, "CHR Write (0x%04x) out of bounds", addr);
            break;
    }
}
