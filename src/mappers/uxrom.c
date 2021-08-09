#include "mapper.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);

void load_UXROM(Mapper* mapper){
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    // last bank offset
    mapper->clamp = (mapper->PRG_banks - 1) * 0x4000;
    mapper->PRG_ptr = mapper->PRG_ROM;
}


static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    if(address < 0xC000)
        return *(mapper->PRG_ptr + (address - 0x8000));
    else
        return mapper->PRG_ROM[mapper->clamp + (address - 0xC000)];
}


static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    mapper->PRG_ptr = mapper->PRG_ROM + (value & 0x7) * 0x4000;
}
