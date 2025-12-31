#include "mapper.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static void write_PRG_UN1ROM(Mapper*, uint16_t, uint8_t);

void load_UXROM(Mapper* mapper){
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    // last bank offset
    mapper->clamp = (mapper->PRG_banks - 1) * 0x4000;
    mapper->PRG_ptrs[0] = mapper->PRG_ROM;
}

void load_UN1ROM(Mapper* mapper) {
    load_UXROM(mapper);
    mapper->write_PRG = write_PRG_UN1ROM;
}


static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    if(address < 0xC000)
        return *(mapper->PRG_ptrs[0] + (address - 0x8000));
    return mapper->PRG_ROM[mapper->clamp + (address - 0xC000)];
}


static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    mapper->PRG_ptrs[0] = mapper->PRG_ROM + (value & 0x7) * 0x4000;
}

static void write_PRG_UN1ROM(Mapper* mapper, uint16_t address, uint8_t value) {
    mapper->PRG_ptrs[0] = mapper->PRG_ROM + ((value & 0b11100) >> 2) * 0x4000;
}
