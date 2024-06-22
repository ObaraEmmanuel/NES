#include "mapper.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);

void load_colordreams(Mapper* mapper){
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->PRG_ptr = mapper->PRG_ROM;
    mapper->CHR_ptr = mapper->CHR_ROM;
}

static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    return *(mapper->PRG_ptr + (address - 0x8000));
}

static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    /*
    7  bit  0
    ---- ----
    CCCC LLPP
    |||| ||||
    |||| ||++- Select 32 KB PRG ROM bank for CPU $8000-$FFFF
    |||| ++--- Used for lockout defeat
    ++++------ Select 8 KB CHR ROM bank for PPU $0000-$1FFF
    */
    mapper->PRG_ptr = mapper->PRG_ROM + (value & 0x3) * 0x8000;
    mapper->CHR_ptr = mapper->CHR_ROM + 0x2000 * ((value >> 4) & 0xf);
}


static uint8_t read_CHR(Mapper* mapper, uint16_t address){
    return *(mapper->CHR_ptr + address);
}

