#include <stdlib.h>
#include <string.h>

#include "mapper.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);

void load_GNROM(Mapper* mapper){
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->bank_select = 0;

    if(!mapper->CHR_banks){
        mapper->CHR_RAM = malloc(0x2000);
        memset(mapper->CHR_RAM, 0, 0x2000);
    }
}

static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    // PRG bank determined by bit 5 - 4
    return mapper->PRG_ROM[(address - 0x8000) + ((mapper->bank_select >> 4) & 0x3) * 0x8000];
}


static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    mapper->bank_select = value;
}


static uint8_t read_CHR(Mapper* mapper, uint16_t address){
    // 8k CHR bank selected determined by bit 0 - 1
    return mapper->CHR_RAM[address + 0x2000 * (mapper->bank_select & 0x3)];
}
