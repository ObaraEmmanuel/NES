#include "mapper.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);

void load_AOROM(Mapper* mapper){
    mapper->write_PRG = write_PRG;
    mapper->read_PRG = read_PRG;
    mapper->PRG_ptr = mapper->PRG_ROM;
}

static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    mapper->PRG_ptr = mapper->PRG_ROM + (value & 0x7) * 0x8000;
    if((value >> 4) & 0x1)
        set_mirroring(mapper, ONE_SCREEN_UPPER);
    else
        set_mirroring(mapper, ONE_SCREEN_LOWER);
}

static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    // PRG bank determined by bit 0 - 2
    return *(mapper->PRG_ptr + (address - 0x8000));
}