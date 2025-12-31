#include "mapper.h"

static uint8_t read_PRG(Mapper *mapper, uint16_t address);
static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper *mapper, uint16_t address);

int load_GNROM(Mapper *mapper) {
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->PRG_ptrs[0] = mapper->PRG_ROM;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    return 0;
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    return *(mapper->PRG_ptrs[0] + (address - 0x8000));
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    // PRG bank determined by bit 5 - 4
    mapper->PRG_ptrs[0] = mapper->PRG_ROM + ((value >> 4) & 0x3) * 0x8000;
    // 8k CHR bank selected determined by bit 0 - 1
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + 0x2000 * (value & 0x3);
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return *(mapper->CHR_ptrs[0] + address);
}
