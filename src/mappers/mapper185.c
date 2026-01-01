#include "emulator.h"
#include "mapper.h"
#include "utils.h"

static void write_PRG(Mapper * mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper * mapper, uint16_t address);
static void write_CHR(Mapper * mapper, uint16_t address, uint8_t value);

int load_mapper185(Mapper *mapper) {
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->CHR_ptrs[0] = mapper->CHR_ROM;
    // where CS1/CS2 value is unknown, enable CHR_ROM
    if (mapper->submapper == 0)
        mapper->CHR_regs[0] = 1;
    return 0;
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    // 8k CHR bank selected determined by bit 0 - 1
    switch (mapper->submapper) {
        case 0: mapper->CHR_regs[0] = 1; break;
        case 4: mapper->CHR_regs[0] = (value & 0x3) == 0; break;
        case 5: mapper->CHR_regs[0] = (value & 0x3) == 1; break;
        case 6: mapper->CHR_regs[0] = (value & 0x3) == 2; break;
        case 7: mapper->CHR_regs[0] = (value & 0x3) == 3; break;
        default: break;
    }
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    if (!mapper->CHR_regs[0])
        // CHR_ROM not enabled, return open bus
        return mapper->emulator->mem.bus;
    return mapper->CHR_ROM[address];
}

static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value) {
    LOG(DEBUG, "Attempted to write to CHR-ROM");
}
