#include <stdlib.h>
#include <string.h>

#include "mapper.h"
#include "utils.h"

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);

void load_CNROM(Mapper* mapper){
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->bank_select = 0;

    if(!mapper->CHR_banks){
        mapper->CHR_RAM = malloc(0x2000);
        memset(mapper->CHR_RAM, 0, 0x2000);
    }
}


static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    return mapper->PRG_ROM[(address - 0x8000) % (0x4000 * mapper->PRG_banks)];
}


static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    // 8k CHR bank selected determined by bit 0 - 1
    mapper->bank_select = value & 0x3;
}


static uint8_t read_CHR(Mapper* mapper, uint16_t address){
    return mapper->CHR_RAM[address + 0x2000 * mapper->bank_select];
}


static void write_CHR(Mapper* mapper, uint16_t address, uint8_t value){
    if(mapper->CHR_banks){
        LOG(DEBUG, "Attempted to write to CHR-ROM");
    }
    mapper->CHR_RAM[address] = value;
}
