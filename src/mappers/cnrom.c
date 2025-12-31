#include "mapper.h"
#include "utils.h"

static void write_PRG(Mapper *, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper *, uint16_t);
static void write_CHR(Mapper *, uint16_t, uint8_t);

int load_CNROM(Mapper *mapper) {
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    return 0;
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    // 8k CHR bank selected determined by bit 0 - 1
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + 0x2000 * (value & 0x3);
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return *(mapper->CHR_ptrs[0] + address);
}

static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value) {
    LOG(DEBUG, "Attempted to write to CHR-ROM");
}
