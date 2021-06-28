#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapper.h"
#include "utils.h"

static void select_mapper(Mapper*  mapper);
static void set_mapping(Mapper* mapper, uint16_t tr, uint16_t tl, uint16_t br, uint16_t bl);

// generic mapper implementations
static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);


static void select_mapper(Mapper* mapper){
    // load generic implementations
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;

    switch (mapper->mapper_num) {
        case NROM:
            // generic implementation will suffice
            break;
        case UXROM:
            load_UXROM(mapper);
            break;
        case MMC1:
            load_MMC1(mapper);
            break;
        case CNROM:
            load_CNROM(mapper);
            break;
        case MMC3:
        default:
            LOG(ERROR, "Mapper no %u not implemented", mapper->mapper_num);
            exit(EXIT_FAILURE);
    }
}


static void set_mapping(Mapper* mapper, uint16_t tr, uint16_t tl, uint16_t br, uint16_t bl){
    mapper->name_table_map[0] = tr;
    mapper->name_table_map[1] = tl;
    mapper->name_table_map[2] = br;
    mapper->name_table_map[3] = bl;
}


void set_mirroring(Mapper* mapper, Mirroring mirroring){
    switch (mirroring) {
        case HORIZONTAL:
            set_mapping(mapper, 0, 0, 0x400, 0x400);
            LOG(DEBUG, "Using mirroring: Horizontal");
            break;
        case VERTICAL:
            set_mapping(mapper,0, 0x400, 0, 0x400);
            LOG(DEBUG, "Using mirroring: Vertical");
            break;
        case ONE_SCREEN_LOWER:
        case ONE_SCREEN:
            set_mapping(mapper,0, 0, 0, 0);
            LOG(DEBUG, "Using mirroring: Single screen lower");
            break;
        case ONE_SCREEN_UPPER:
            set_mapping(mapper, 0x400, 0x400, 0x400, 0x400);
            LOG(DEBUG, "Using mirroring: Single screen upper");
            break;
        default:
            set_mapping(mapper,0, 0, 0, 0);
            LOG(ERROR, "Unknown mirroring %u", mirroring);
    }
}


static uint8_t read_PRG(Mapper* mapper, uint16_t address){
    return mapper->PRG_ROM[(address - 0x8000) % (0x4000 * mapper->PRG_banks)];
}


static void write_PRG(Mapper* mapper, uint16_t address, uint8_t value){
    LOG(DEBUG, "Attempted to write to PRG-ROM");
}


static uint8_t read_CHR(Mapper* mapper, uint16_t address){
    return mapper->CHR_RAM[address];
}


static void write_CHR(Mapper* mapper, uint16_t address, uint8_t value){
    if(mapper->CHR_banks){
        LOG(DEBUG, "Attempted to write to CHR-ROM");
        return;
    }
    mapper->CHR_RAM[address] = value;
}


void load_file(char* file_name, Mapper* mapper){
    FILE* file = fopen(file_name, "rb");

    if(file == NULL){
        LOG(ERROR, "file '%s' not found", file_name);
        exit(EXIT_FAILURE);
    }

    uint8_t header[INES_HEADER_SIZE];
    fread(header, 1, INES_HEADER_SIZE, file);

    if(strncmp((char *)header, "NES\x1A", 4) != 0){
        LOG(ERROR, "unknown file format");
        exit(EXIT_FAILURE);
    }

    mapper->PRG_banks = header[4];
    mapper->CHR_banks = header[5];

    LOG(INFO, "PRG banks (16KB): %u", mapper->PRG_banks);
    LOG(INFO, "CHR banks (8KB): %u", mapper->CHR_banks);

    if(header[6] & BIT_1){
        mapper->save_RAM = malloc(0x2000);
        LOG(INFO, "Battery backed save RAM 8KB : Available");
    }

    if(header[6] & BIT_2) {
        LOG(ERROR, "Trainer not supported");
        exit(EXIT_FAILURE);
    }

    if(header[6] & BIT_3){
        mapper->mirroring = FOUR_SCREEN;
    }
    else if(header[6] & BIT_0) {
        mapper->mirroring = VERTICAL;
    }
    else {
        mapper->mirroring = HORIZONTAL;
    }

    mapper->mapper_num = ((header[6] & 0xF0) >> 4) | (header[7] & 0xF0);

    LOG(INFO, "Using mapper #%d", mapper->mapper_num);

    mapper->RAM_banks = header[8];

    if(mapper->RAM_banks == 0)
        LOG(INFO, "SRAM Banks (8kb): Not specified");
    else
        LOG(INFO, "SRAM Banks (8kb): %u (Not used by emulator)", mapper->RAM_banks);


    if((header[10] & 0x3) == 0x2 || (header[10] & BIT_0))
        LOG(INFO, "ROM type: PAL");
    else
        LOG(INFO, "ROM type: NTSC");

    mapper->PRG_ROM = malloc(0x4000 * mapper->PRG_banks);
    fread(mapper->PRG_ROM, 1, 0x4000 * mapper->PRG_banks, file);

    if(mapper->CHR_banks) {
        mapper->CHR_RAM = malloc(0x2000 * mapper->CHR_banks);
        fread(mapper->CHR_RAM, 1, 0x2000 * mapper->CHR_banks, file);
    }

    fclose(file);

    select_mapper(mapper);
    set_mirroring(mapper, mapper->mirroring);
}

void free_mapper(Mapper* mapper){
    if(mapper->PRG_ROM != NULL)
        free(mapper->PRG_ROM);
    if(mapper->CHR_RAM != NULL)
        free(mapper->CHR_RAM);
    if(mapper->extension != NULL)
        free(mapper->extension);
}