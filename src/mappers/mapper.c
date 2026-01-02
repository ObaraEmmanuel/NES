#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "mapper.h"
#include "emulator.h"
#include "genie.h"
#include "utils.h"
#include "nsf.h"

static int select_mapper(Mapper *mapper);
static void set_mapping(Mapper *mapper, uint16_t tl, uint16_t tr, uint16_t bl, uint16_t br);

// generic mapper implementations
static uint8_t read_PRG(Mapper *mapper, uint16_t address);
static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_CHR(Mapper *mapper, uint16_t address);
static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value);
static uint8_t read_ROM(Mapper *mapper, uint16_t address);
static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value);
static void on_scanline(Mapper *mapper);

static int select_mapper(Mapper *mapper) {
    // load generic implementations
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->read_ROM = read_ROM;
    mapper->write_ROM = write_ROM;
    mapper->on_scanline = on_scanline;
    mapper->clamp = (mapper->PRG_banks * 0x4000) - 1;

    switch (mapper->mapper_num) {
        case 0: return 0;
        case 1: return load_MMC1(mapper);
        case 2: return load_UXROM(mapper);
        case 3: return load_CNROM(mapper);
        case 4: return load_MMC3(mapper);
        case 7: return load_AOROM(mapper);
        case 11: return load_colordreams(mapper);
        case 13: return load_CPROM(mapper);
        case 46: return load_colordreams46(mapper);
        case 66: return load_GNROM(mapper);
        case 75: return load_VRC1(mapper);
        case 94: return load_UN1ROM(mapper);
        case 180: return load_mapper180(mapper);
        case 185: return load_mapper185(mapper);
        default:
            LOG(ERROR, "Mapper no %u not implemented", mapper->mapper_num);
            return -1;
    }
}

static void set_mapping(Mapper *mapper, uint16_t tl, uint16_t tr, uint16_t bl, uint16_t br) {
    mapper->name_table_map[0] = tl;
    mapper->name_table_map[1] = tr;
    mapper->name_table_map[2] = bl;
    mapper->name_table_map[3] = br;
}

void set_mirroring(Mapper *mapper, Mirroring mirroring) {
    if (mirroring == mapper->mirroring)
        return;
    switch (mirroring) {
        case HORIZONTAL:
            set_mapping(mapper, 0, 0, 0x400, 0x400);
            LOG(DEBUG, "Using mirroring: Horizontal");
            break;
        case VERTICAL:
            set_mapping(mapper, 0, 0x400, 0, 0x400);
            LOG(DEBUG, "Using mirroring: Vertical");
            break;
        case ONE_SCREEN_LOWER:
        case ONE_SCREEN:
            set_mapping(mapper, 0, 0, 0, 0);
            LOG(DEBUG, "Using mirroring: Single screen lower");
            break;
        case ONE_SCREEN_UPPER:
            set_mapping(mapper, 0x400, 0x400, 0x400, 0x400);
            LOG(DEBUG, "Using mirroring: Single screen upper");
            break;
        case FOUR_SCREEN:
            set_mapping(mapper, 0, 0x400, 0x800, 0xC00);
            LOG(DEBUG, "Using mirroring: Four screen");
            break;
        default:
            set_mapping(mapper, 0, 0, 0, 0);
            LOG(ERROR, "Unknown mirroring %u", mirroring);
    }
    mapper->mirroring = mirroring;
}

static void on_scanline(Mapper *mapper) {
}

static uint8_t read_ROM(Mapper *mapper, uint16_t address) {
    if (address < 0x6000) {
        // expansion rom
        LOG(DEBUG, "Attempted to read from unavailable expansion ROM");
        return mapper->emulator->mem.bus;
    }
    if (address < 0x8000) {
        // PRG ram
        if (mapper->PRG_RAM != NULL)
            return mapper->PRG_RAM[address - 0x6000];

        LOG(DEBUG, "Attempted to read from non existent PRG RAM");
        return mapper->emulator->mem.bus;
    }

    // PRG
    return mapper->read_PRG(mapper, address);
}

static void write_ROM(Mapper *mapper, uint16_t address, uint8_t value) {
    if (address < 0x6000) {
        LOG(DEBUG, "Attempted to write to unavailable expansion ROM");
        return;
    }

    if (address < 0x8000) {
        // extended ram
        if (mapper->PRG_RAM != NULL)
            mapper->PRG_RAM[address - 0x6000] = value;
        else {
            LOG(DEBUG, "Attempted to write to non existent PRG RAM");
        }
        return;
    }

    // PRG
    mapper->write_PRG(mapper, address, value);
}

static uint8_t read_PRG(Mapper *mapper, uint16_t address) {
    return mapper->PRG_ROM[(address - 0x8000) & mapper->clamp];
}

static void write_PRG(Mapper *mapper, uint16_t address, uint8_t value) {
    LOG(DEBUG, "Attempted to write to PRG-ROM");
}

static uint8_t read_CHR(Mapper *mapper, uint16_t address) {
    return mapper->CHR_ROM[address];
}

static void write_CHR(Mapper *mapper, uint16_t address, uint8_t value) {
    if (!mapper->CHR_RAM_size) {
        LOG(DEBUG, "Attempted to write to CHR-ROM");
        return;
    }
    mapper->CHR_ROM[address] = value;
}

static long long read_file_to_buffer(char *file_name, uint8_t **buffer) {
    SDL_IOStream *file = SDL_IOFromFile(file_name, "rb");

    if (file == NULL) {
        LOG(ERROR, "file '%s' not found", file_name);
        return -1;
    }

    long long f_size = SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    if (f_size < 0) {
        LOG(ERROR, "Error reading length for file %s", file_name);
        return -1;
    }
    SDL_SeekIO(file, 0, SDL_IO_SEEK_SET);

    *buffer = malloc(f_size);
    if (*buffer == NULL) {
        LOG(ERROR, "Error allocating memory for %s", file_name);
        return -1;
    }

    size_t input_bytes = SDL_ReadIO(file, *buffer, f_size);
    if (input_bytes != f_size) {
        free(*buffer);
        LOG(ERROR, "Error reading file %s", file_name);
        return -1;
    }

    return f_size;
}

void load_file(char *file_name, char *game_genie, Mapper *mapper) {
    ROMData data = {0};
    long long rom_size = read_file_to_buffer(file_name, &data.rom);
    if (rom_size < 0) {
        quit(EXIT_FAILURE);
    }

    data.rom_size = rom_size;
    data.rom_name = file_name;

    if (game_genie != NULL) {
        rom_size = read_file_to_buffer(game_genie, &data.genie_rom);
        if (rom_size < 0) {
            quit(EXIT_FAILURE);
        }
        data.genie_rom_size = rom_size;
    }

    const int result = load_data(&data, mapper);
    if (data.rom != NULL)
        free(data.rom);
    if (data.genie_rom != NULL)
        free(data.genie_rom);

    if (result < 0) {
        quit(EXIT_FAILURE);
    }
}

int load_data(ROMData *rom_data, Mapper *mapper) {
    // clear mapper
    memset(mapper, 0, sizeof(Mapper));
    const uint8_t *header = rom_data->rom;
    size_t offset = INES_HEADER_SIZE;

    if (strncmp((char *) header, "NESM\x1A", 5) == 0) {
        LOG(INFO, "Using NSF format");
        return load_nsf(rom_data, mapper);
    }

    if (strncmp((char *) header, "NSFE", 4) == 0) {
        LOG(INFO, "Using NSFe format");
        return load_nsfe(rom_data, mapper);
    }

    if (strncmp((char *) header, "NES\x1A", 4) != 0) {
        LOG(ERROR, "unknown file format");
        return -1;
    }

    uint8_t mapnum = header[7] & 0x0C;
    if (mapnum == 0x08) {
        mapper->format = NES2;
        LOG(INFO, "Using NES2.0 format");
    }

    uint8_t last_4_zeros = 1;
    for (size_t i = 12; i < INES_HEADER_SIZE; i++) {
        if (header[i] != 0) {
            last_4_zeros = 0;
            break;
        }
    }

    if (mapnum == 0x00 && last_4_zeros) {
        mapper->format = INES;
        LOG(INFO, "Using iNES format");
    } else if (mapnum == 0x04) {
        mapper->format = ARCHAIC_INES;
        LOG(INFO, "Using iNES (archaic) format");
    } else if (mapnum != 0x08) {
        mapper->format = ARCHAIC_INES;
        LOG(INFO, "Possibly using iNES (archaic) format");
    }

    mapper->PRG_banks = header[4];
    mapper->CHR_banks = header[5];

    if (header[6] & BIT_1) {
        LOG(INFO, "Uses Battery backed save RAM 8KB");
    }

    if (header[6] & BIT_2) {
        LOG(ERROR, "Trainer not supported");
        return -1;
    }

    Mirroring mirroring;

    if (header[6] & BIT_3) {
        mirroring = FOUR_SCREEN;
    } else if (header[6] & BIT_0) {
        mirroring = VERTICAL;
    } else {
        mirroring = HORIZONTAL;
    }
    mapper->mapper_num = (header[6] & 0xF0) >> 4;
    mapper->type = NTSC;

    if (mapper->format == INES) {
        mapper->mapper_num |= header[7] & 0xF0;
        mapper->RAM_banks = header[8];

        if (mapper->RAM_banks) {
            mapper->RAM_size = 0x2000 * mapper->RAM_banks;
            LOG(INFO, "PRG RAM Banks (8kb): %u", mapper->RAM_banks);
        }

        if (header[9] & 1) {
            mapper->type = PAL;
        } else {
            mapper->type = NTSC;
        }
    } else if (mapper->format == NES2) {
        mapper->mapper_num |= header[7] & 0xF0;
        mapper->mapper_num |= ((header[8] & 0xf) << 8);
        mapper->submapper = header[8] >> 4;

        mapper->PRG_banks |= ((header[9] & 0x0f) << 8);
        mapper->CHR_banks |= ((header[9] & 0xf0) << 4);

        if (header[10] & 0xf)
            mapper->RAM_size = (64 << (header[10] & 0xf));
        if (header[10] & 0xf0)
            mapper->RAM_size += (64 << ((header[10] & 0xf0) >> 4));
        if (mapper->RAM_size)
            LOG(INFO, "PRG-RAM size: %u", mapper->RAM_size);

        if (header[11] & 0xf)
            mapper->CHR_RAM_size = (64 << (header[11] & 0xf));
        if (header[11] & 0xf0)
            mapper->CHR_RAM_size += (64 << ((header[11] & 0xf0) >> 4));
        if (mapper->CHR_RAM_size)
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
                return -1;
            default:
                break;
        }
    }

    if (!mapper->RAM_banks && mapper->format != NES2) {
        LOG(INFO, "PRG RAM Banks (8kb): Not specified, Assuming 8kb");
        mapper->RAM_size = 0x2000;
    }

    if (mapper->RAM_size) {
        mapper->PRG_RAM = malloc(mapper->RAM_size);
        memset(mapper->PRG_RAM, 0, mapper->RAM_size);
    }

    if (mapper->format != NES2) {
        if (!mapper->CHR_banks) {
            mapper->CHR_RAM_size = 0x2000;
            LOG(INFO, "CHR-ROM Not specified, Assuming 8kb CHR-RAM");
        }

        if (rom_data->rom_name != NULL) {
            if (strstr(rom_data->rom_name, "(E)") != NULL && mapper->type == NTSC) {
                // probably PAL ROM
                mapper->type = PAL;
            }
        }
    }

    LOG(INFO, "PRG banks (16KB): %u", mapper->PRG_banks);
    LOG(INFO, "CHR banks (8KB): %u", mapper->CHR_banks);

    mapper->PRG_ROM = malloc(0x4000 * mapper->PRG_banks);
    memcpy(mapper->PRG_ROM, rom_data->rom + offset, 0x4000 * mapper->PRG_banks);
    offset += 0x4000 * mapper->PRG_banks;

    if (mapper->CHR_banks) {
        mapper->CHR_ROM = malloc(0x2000 * mapper->CHR_banks);
        memcpy(mapper->CHR_ROM, rom_data->rom + offset, 0x2000 * mapper->CHR_banks);
        // offset += 0x2000 * mapper->CHR_banks;
    } else {
        if (!mapper->CHR_RAM_size) {
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

    LOG(INFO, "Using mapper #%d: sub-mapper #%d", mapper->mapper_num, mapper->submapper);

    if (select_mapper(mapper) < 0)
        return -1;
    set_mirroring(mapper, mirroring);

    if (rom_data->genie_rom != NULL) {
        LOG(INFO, "-------- Game Genie Cartridge info ---------");
        return load_genie(rom_data, mapper);
    }
    return 0;
}

void free_mapper(Mapper *mapper) {
    if (mapper->PRG_ROM != NULL)
        free(mapper->PRG_ROM);
    if (mapper->CHR_ROM != NULL)
        free(mapper->CHR_ROM);
    if (mapper->PRG_RAM != NULL)
        free(mapper->PRG_RAM);
    if (mapper->extension != NULL)
        free(mapper->extension);
    if (mapper->genie != NULL)
        free(mapper->genie);
    if (mapper->NSF != NULL) {
        free_NSF(mapper->NSF);
    }
    LOG(DEBUG, "Mapper cleanup complete");
}
