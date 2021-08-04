#include <stdlib.h>
#include <string.h>

#include "mapper.h"
#include "utils.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);

void load_AOROM(Mapper* mapper){
    mapper->write_PRG = write_PRG;
    mapper->read_PRG = read_PRG;
    mapper->bank_select = 0;

    if(!mapper->CHR_banks){
        mapper->CHR_RAM = malloc(0x2000);
        memset(mapper->CHR_RAM, 0, 0x2000);
    }
}

static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    mapper->bank_select = value & 0x7;
    if((value >> 4) & 0x1)
        set_mirroring(mapper, ONE_SCREEN_UPPER);
    else
        set_mirroring(mapper, ONE_SCREEN_LOWER);
}

static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    // PRG bank determined by bit 0 - 2
    return mapper->PRG_ROM[(address - 0x8000) + (mapper->bank_select * 0x8000)];
}