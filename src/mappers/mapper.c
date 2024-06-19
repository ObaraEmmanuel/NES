#include <stdlib.h>
#include <string.h>
#include <SDL_rwops.h>

#include "mapper.h"
#include "emulator.h"
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
    if(mirroring == mapper->mirroring)
        return;
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
    mapper->mirroring = mirroring;
}

static uint8_t read_ROM(Mapper* mapper, uint16_t address){
    if(address < 0x6000) {
        // expansion rom
        LOG(DEBUG, "Attempted to read from unavailable expansion ROM");
        return mapper->emulator->mem.bus;
    }
    if(address < 0x8000) {
        // PRG ram
        if(mapper->PRG_RAM != NULL)
            return mapper->PRG_RAM[address - 0x6000];

        LOG(DEBUG, "Attempted to read from non existent PRG RAM");
        return mapper->emulator->mem.bus;
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
        if(mapper->PRG_RAM != NULL)
            mapper->PRG_RAM[address - 0x6000] = value;
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
    return mapper->CHR_ROM[address];
}


static void write_CHR(Mapper* mapper, uint16_t address, uint8_t value){
    if(!mapper->CHR_RAM_size){
        LOG(DEBUG, "Attempted to write to CHR-ROM");
        return;
    }
    mapper->CHR_ROM[address] = value;
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
        LOG(INFO, "Using NES2.0 format");
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
    } else if(mapnum !=0x08) {
        mapper->format = ARCHAIC_INES;
        LOG(INFO, "Possibly using iNES (archaic) format");
    }

    mapper->PRG_banks = header[4];
    mapper->CHR_banks = header[5];

    if(header[6] & BIT_1){
        LOG(INFO, "Uses Battery backed save RAM 8KB");
    }

    if(header[6] & BIT_2) {
        LOG(ERROR, "Trainer not supported");
        exit(EXIT_FAILURE);
    }

    Mirroring mirroring;

    if(header[6] & BIT_3){
        mirroring = FOUR_SCREEN;
    }
    else if(header[6] & BIT_0) {
        mirroring = VERTICAL;
    }
    else {
        mirroring = HORIZONTAL;
    }
    mapper->mapper_num = (header[6] & 0xF0) >> 4;
    mapper->type = NTSC;

    if(mapper->format == INES) {
        mapper->mapper_num |= header[7] & 0xF0;
        mapper->RAM_banks = header[8];

        if(mapper->RAM_banks == 0) {
            LOG(INFO, "SRAM Banks (8kb): Not specified, Assuming 8kb");
            mapper->RAM_size = 0x2000;
        }else {
            mapper->RAM_size = 0x2000 * mapper->RAM_banks;
            LOG(INFO, "SRAM Banks (8kb): %u", mapper->RAM_banks);
        }

        if(header[9] & 1){
            mapper->type = PAL;
        }else {
            mapper->type = NTSC;
        }
    } else if(mapper->format == NES2) {
        mapper->mapper_num |= header[7] & 0xF0;
        mapper->mapper_num |= ((header[8] & 0xf) << 8);
        mapper->submapper = header[8] >> 4;

        mapper->PRG_banks |= ((header[9] & 0x0f) << 8);
        mapper->CHR_banks |= ((header[9] & 0xf0) << 4);

        if(header[10] & 0xf)
            mapper->RAM_size = (64 << (header[10] & 0xf));
        if(header[10] & 0xf0)
            mapper->RAM_size += (64 << ((header[10] & 0xf0) >> 4));
        if(mapper->RAM_size)
            LOG(INFO, "PRG-RAM size: %u", mapper->RAM_size);

        if(header[11] & 0xf)
            mapper->CHR_RAM_size = (64 << (header[11] & 0xf));
        if(header[11] & 0xf0)
            mapper->CHR_RAM_size += (64 << ((header[11] & 0xf0) >> 4));
        if(mapper->CHR_RAM_size)
            LOG(INFO, "CHR-RAM size: %u", mapper->CHR_RAM_size);

        switch (header[12] & 0x3) {
            case 0:
                mapper->type = NTSC;
                break;
            case 1:
                mapper->type = PAL;
                break;
            case 2:
                // multi-region
                mapper->type = DUAL;
                break;
            case 3:
                mapper->type = DENDY;
                LOG(ERROR, "Dendy ROM not supported");
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }

    if(mapper->RAM_size) {
        mapper->PRG_RAM = malloc(mapper->RAM_size);
        memset(mapper->PRG_RAM, 0, mapper->RAM_size);
    }

    if(!mapper->CHR_banks && mapper->format != NES2) {
        mapper->CHR_RAM_size = 0x2000;
        LOG(INFO, "CHR-ROM Not specified, Assuming 8kb CHR-RAM");
    }

    if(mapper->format != NES2) {

        if(!mapper->CHR_banks) {
            mapper->CHR_RAM_size = 0x2000;
            LOG(INFO, "CHR-ROM Not specified, Assuming 8kb CHR-RAM");
        }

        if(strstr(file_name, "(E)") != NULL && mapper->type == NTSC) {
            // probably PAL ROM
            mapper->type = PAL;
        }
    }

    LOG(INFO, "PRG banks (16KB): %u", mapper->PRG_banks);
    LOG(INFO, "CHR banks (8KB): %u", mapper->CHR_banks);

    mapper->PRG_ROM = malloc(0x4000 * mapper->PRG_banks);
    SDL_RWread(file, mapper->PRG_ROM, 0x4000 * mapper->PRG_banks, 1);

    if(mapper->CHR_banks) {
        mapper->CHR_ROM = malloc(0x2000 * mapper->CHR_banks);
        SDL_RWread(file, mapper->CHR_ROM, 0x2000 * mapper->CHR_banks, 1);
    }else{
        if(!mapper->CHR_RAM_size) {
            LOG(INFO, "No CHR-RAM or CHR-ROM specified, Using 8kb CHR-RAM");
            mapper->CHR_RAM_size = 0x2000;
        }
        mapper->CHR_ROM = malloc(mapper->CHR_RAM_size);
        memset(mapper->CHR_ROM, 0, mapper->CHR_RAM_size);
    }

    switch (mapper->type) {
        case NTSC:
            LOG(INFO, "ROM type: NTSC");
            break;
        case DUAL:
            LOG(INFO, "ROM type: DUAL (Using NTSC)");
            mapper->type = NTSC;
            break;
        case PAL:
            LOG(INFO, "ROM type: PAL");
            break;
        default:
            LOG(INFO, "ROM type: Unknown");
    }

    LOG(INFO, "Using mapper #%d", mapper->mapper_num);
    select_mapper(mapper);
    set_mirroring(mapper, mirroring);

    if(game_genie != NULL){
        LOG(INFO, "-------- Game Genie Cartridge info ---------");
        load_genie(game_genie, mapper);
    }
    SDL_RWclose(file);
}

void free_mapper(Mapper* mapper){
    if(mapper->PRG_ROM != NULL)
        free(mapper->PRG_ROM);
    if(mapper->CHR_ROM != NULL)
        free(mapper->CHR_ROM);
    if(mapper->PRG_RAM != NULL)
        free(mapper->PRG_RAM);
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