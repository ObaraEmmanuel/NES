#include "mapper.h"
#include "utils.h"

static void write_PRG(Mapper * mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper * mapper, uint16_t address);
static void write_CHR(Mapper * mapper, uint16_t address, uint8_t value);

int load_CNROM(Mapper *mapper) {
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    // store chr mask in chr reg 0
    // the mask ensures CHR wraps around correctly
    mapper->CHR_regs[0] = next_power_of_2(mapper->CHR_banks) - 1;
    if (mapper->submapper == 2)
        LOG(WARN, "Requires unimplemented AND bus conflicts");
    return 0;
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    // 8k CHR bank selected determined by value
    mapper->CHR_ptrs[0] = mapper->CHR_ROM + 0x2000 * (value & mapper->CHR_regs[0]);
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return *(mapper->CHR_ptrs[0] + address);
}

static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value) {
    LOG(DEBUG, "Attempted to write to CHR-ROM");
}
