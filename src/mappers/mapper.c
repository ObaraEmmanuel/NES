#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_rwops.h>

#include "mapper.h"
#include "genie.h"
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
    mapper->clamp = (mapper->PRG_banks * 0x4000) - 1;

    if(!mapper->CHR_banks) {
        mapper->CHR_RAM = malloc(0x2000);
        memset(mapper->CHR_RAM, 0, 0x2000);
    }

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
        case GNROM:
            load_GNROM(mapper);
            break;
        case AOROM:
            load_AOROM(mapper);
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
    return mapper->PRG_ROM[(address - 0x8000) & mapper->clamp];
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


void load_file(char* file_name, char* game_genie, Mapper* mapper){
    SDL_RWops *file;
    file = SDL_RWFromFile(file_name, "rb");

    if(file == NULL){
        LOG(ERROR, "file '%s' not found", file_name);
        exit(EXIT_FAILURE);
    }

    // clear mapper
    memset(mapper, 0, sizeof(Mapper));

    uint8_t header[INES_HEADER_SIZE];
    SDL_RWread(file, header, INES_HEADER_SIZE, 1);

    if(strncmp((char *)header, "NES\x1A", 4) != 0){
        LOG(ERROR, "unknown file format");
        exit(EXIT_FAILURE);
    }

    if((header[7]&0x0C)==0x08){
        LOG(ERROR, "NES2.0 format not supported");
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

    switch (header[10] & 0x3) {
        case 0:
            if(header[9] & 1){
                mapper->type = PAL;
                LOG(INFO, "ROM type: PAL");
            }else {
                mapper->type = NTSC;
                LOG(INFO, "ROM type: NTSC");
            }
            break;
        case 2:
            mapper->type = PAL;
            LOG(INFO, "ROM type: PAL");
            break;
        case 1:
        case 3:
            mapper->type = NTSC;
            LOG(INFO, "ROM type: Dual compatible, using NTSC");
            break;
    }

    mapper->PRG_ROM = malloc(0x4000 * mapper->PRG_banks);
    SDL_RWread(file, mapper->PRG_ROM, 0x4000 * mapper->PRG_banks, 1);

    if(mapper->CHR_banks) {
        mapper->CHR_RAM = malloc(0x2000 * mapper->CHR_banks);
        SDL_RWread(file, mapper->CHR_RAM, 0x2000 * mapper->CHR_banks, 1);
    }else
        mapper->CHR_RAM = NULL;

    SDL_RWclose(file);

    select_mapper(mapper);
    set_mirroring(mapper, mapper->mirroring);

    if(game_genie != NULL){
        LOG(INFO, "-------- Game Genie Cartridge info ---------");
        load_genie(game_genie, mapper);
    }
}

void free_mapper(Mapper* mapper){
    if(mapper->PRG_ROM != NULL)
        free(mapper->PRG_ROM);
    if(mapper->CHR_RAM != NULL)
        free(mapper->CHR_RAM);
    if(mapper->extension != NULL)
        free(mapper->extension);
    if(mapper->genie != NULL)
        free(mapper->genie);
    LOG(DEBUG, "Mapper cleanup complete");
}