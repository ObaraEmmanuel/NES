#include "mapper.h"
#include "utils.h"

static uint8_t read_PRG(Mapper *mapper, uint16_t address);
static void write_PRG(Mapper *mapper, uint16_t address, uint8_t val);
static uint8_t read_CHR(Mapper *mapper, uint16_t address);
static void update_CHR_ptrs(Mapper *mapper);

typedef struct VCR1 {
    uint8_t CHR_0;
    uint8_t CHR_1;
} VRC1_t;

int load_VRC1(Mapper *mapper) {
    VRC1_t *vrc1 = calloc(1, sizeof(VRC1_t));
    mapper->extension = vrc1;
    mapper->write_PRG = write_PRG;
    mapper->read_PRG = read_PRG;
    mapper->read_CHR = read_CHR;
    // Fix last 8k bank
    mapper->PRG_ptrs[3] = mapper->PRG_ROM + (mapper->PRG_banks * 2 - 1) * 0x2000;
    // Init all PRG banks to first bank
    mapper->PRG_ptrs[0] = mapper->PRG_ptrs[1] = mapper->PRG_ptrs[2] = mapper->PRG_ROM;
    // Initialize both banks to the first bank
    vrc1->CHR_0 = vrc1->CHR_1 = 0;
    update_CHR_ptrs(mapper);
    return 0;
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t val) {
    VRC1_t *vrc1 = mapper->extension;

    switch (address & 0xf000) {
        case 0x8000:
            mapper->PRG_ptrs[0] = mapper->PRG_ROM + (val & 0xf) * 0x2000;
            break;
        case 0x9000:
            if (mapper->mirroring != FOUR_SCREEN) {
                if (val & BIT_1)
                    set_mirroring(mapper, HORIZONTAL);
                else
                    set_mirroring(mapper, VERTICAL);
            }
            vrc1->CHR_0 = (vrc1->CHR_0 & 0xf) | ((val & BIT_1) << 3);
            vrc1->CHR_1 = (vrc1->CHR_1 & 0xf) | ((val & BIT_2) << 2);
            update_CHR_ptrs(mapper);
            break;
        case 0xa000:
            mapper->PRG_ptrs[1] = mapper->PRG_ROM + (val & 0xf) * 0x2000;
            break;
        case 0xc000:
            mapper->PRG_ptrs[2] = mapper->PRG_ROM + (val & 0xf) * 0x2000;
            break;
        case 0xe000:
            vrc1->CHR_0 = (vrc1->CHR_0 & ~0xf) | (val & 0xf);
            update_CHR_ptrs(mapper);
            break;
        case 0xf000:
            vrc1->CHR_1 = (vrc1->CHR_1 & ~0xf) | (val & 0xf);
            update_CHR_ptrs(mapper);
            break;
        default:
            break;
    }
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    return mapper->PRG_ptrs[(address & 0x7fff) >> 13][address & 0x1fff];
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return mapper->CHR_ptrs[address >> 12][address & 0xfff];
}

static void update_CHR_ptrs(Mapper *mapper) {
    VRC1_t *vrc1 = mapper->extension;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + vrc1->CHR_0 * 0x1000;
    mapper->CHR_ptrs[1] = mapper->CHR_ROM + vrc1->CHR_1 * 0x1000;
}
