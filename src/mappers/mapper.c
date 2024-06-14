#include <stdlib.h>
#include <string.h>
#include <SDL_rwops.h>

#include "mapper.h"
#include "genie.h"
#include "utils.h"
#include "nsf.h"

static void select_mapper(Mapper*  mapper);
static void set_mapping(Mapper* mapper, uint16_t tr, uint16_t tl, uint16_t br, uint16_t bl);

// generic mapper implementations
static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);
static uint8_t read_ROM(Mapper*, uint16_t);
static void write_ROM(Mapper*, uint16_t, uint8_t);

static void select_mapper(Mapper* mapper){
    // load generic implementations
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->read_ROM = read_ROM;
    mapper->write_ROM = write_ROM;
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
            load_MMC3(mapper);
            break;
        default:
            LOG(ERROR, "Mapper no %u not implemented", mapper->mapper_num);
            exit(EXIT_FAILURE);
    }
}


static void set_mapping(Mapper* mapper, uint16_t tl, uint16_t tr, uint16_t bl, uint16_t br){
    mapper->name_table_map[0] = tl;
    mapper->name_table_map[1] = tr;
    mapper->name_table_map[2] = bl;
    mapper->name_table_map[3] = br;
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
        case FOUR_SCREEN:
            set_mapping(mapper, 0, 0x400, 0xB00, 0xC00);
            LOG(DEBUG, "Using mirroring: Single screen upper");
            break;
        default:
            set_mapping(mapper,0, 0, 0, 0);
            LOG(ERROR, "Unknown mirroring %u", mirroring);
    }
}

static uint8_t read_ROM(Mapper* mapper, uint16_t address){
    if(address < 0x6000) {
        // expansion rom
        LOG(DEBUG, "Attempted to write to unavailable expansion ROM");
        return 0;
    }
    if(address < 0x8000) {
        // PRG ram
        if(mapper->save_RAM != NULL)
            return mapper->save_RAM[address - 0x6000];

        LOG(DEBUG, "Attempted to read from non existent PRG RAM");
        return 0;
    }

    // PRG
    return mapper->read_PRG(mapper, address);
}


static void write_ROM(Mapper* mapper, uint16_t address, uint8_t value){
    if(address < 0x6000){
        LOG(DEBUG, "Attempted to write to unavailable expansion ROM");
        return;
    }

    if(address < 0x8000){
        // extended ram
        if(mapper->save_RAM != NULL)
            mapper->save_RAM[address - 0x6000] = value;
        else {
            LOG(DEBUG, "Attempted to write to non existent save RAM");
        }
        return;
    }

    // PRG
    mapper->write_PRG(mapper, address, value);
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

    if(strncmp(header, "NESM\x1A", 5) == 0){
        LOG(INFO, "Using NSF format");
        load_nsf(file, mapper);
        SDL_RWclose(file);
        return;
    }

    if(strncmp(header, "NES\x1A", 4) != 0){
        LOG(ERROR, "unknown file format");
        exit(EXIT_FAILURE);
    }

    uint8_t mapnum = header[7] & 0x0C;
    if(mapnum == 0x08){
        mapper->format = NES2;
        LOG(ERROR, "NES2.0 format not supported");
        exit(EXIT_FAILURE);
    }

    uint8_t last_4_zeros = 1;
    for(size_t i = 12; i < INES_HEADER_SIZE; i++) {
        if(header[i] != 0) {
            last_4_zeros = 0;
            break;
        }
    }

    if(mapnum == 0x00 && last_4_zeros) {
        mapper->format = INES;
        LOG(INFO, "Using iNES format");
    } else if(mapnum == 0x04) {
        mapper->format = ARCHAIC_INES;
        LOG(INFO, "Using iNES (archaic) format");
    } else {
        mapper->format = ARCHAIC_INES;
        LOG(INFO, "Possibly using iNES (archaic) format");
    }

    mapper->PRG_banks = header[4];
    mapper->CHR_banks = header[5];

    LOG(INFO, "PRG banks (16KB): %u", mapper->PRG_banks);
    LOG(INFO, "CHR banks (8KB): %u", mapper->CHR_banks);

    mapper->PRG_ROM = malloc(0x4000 * mapper->PRG_banks);
    SDL_RWread(file, mapper->PRG_ROM, 0x4000 * mapper->PRG_banks, 1);

    if(mapper->CHR_banks) {
        mapper->CHR_RAM = malloc(0x2000 * mapper->CHR_banks);
        SDL_RWread(file, mapper->CHR_RAM, 0x2000 * mapper->CHR_banks, 1);
    }else
        mapper->CHR_RAM = NULL;

    if(header[6] & BIT_1){
        LOG(INFO, "Uses Battery backed save RAM 8KB");
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
    mapper->mapper_num = (header[6] & 0xF0) >> 4;
    mapper->type = NTSC;

    if(mapper->format == INES) {
        mapper->mapper_num |= header[7] & 0xF0;
        mapper->RAM_banks = header[8];

        if(mapper->RAM_banks == 0) {
            LOG(INFO, "SRAM Banks (8kb): Not specified, Assuming 8kb");
            mapper->save_RAM = malloc(0x2000);
        }else {
            mapper->save_RAM = malloc(0x2000 * mapper->RAM_banks);
            LOG(INFO, "SRAM Banks (8kb): %u", mapper->RAM_banks);
        }

        if(header[9] & 1){
            mapper->type = PAL;
            LOG(INFO, "ROM type: PAL");
        }else {
            mapper->type = NTSC;
            LOG(INFO, "ROM type: NTSC");
        }
    }

    LOG(INFO, "Using mapper #%d", mapper->mapper_num);
    select_mapper(mapper);
    set_mirroring(mapper, mapper->mirroring);

    if(game_genie != NULL){
        LOG(INFO, "-------- Game Genie Cartridge info ---------");
        load_genie(game_genie, mapper);
    }
    SDL_RWclose(file);
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
    if(mapper->NSF != NULL) {
        SDL_DestroyTexture(mapper->NSF->song_num_tx);
        SDL_DestroyTexture(mapper->NSF->song_info_tx);
        free(mapper->NSF);
    }
    LOG(DEBUG, "Mapper cleanup complete");
}