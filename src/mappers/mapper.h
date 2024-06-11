#pragma once

#include <stdint.h>
#include <stdio.h>

#define INES_HEADER_SIZE 16

typedef enum TVSystem{
    NTSC,
    PAL,
    DENDY
} TVSystem;

typedef enum{
    VERTICAL,
    HORIZONTAL,
    ONE_SCREEN,
    ONE_SCREEN_LOWER,
    ONE_SCREEN_UPPER,
    FOUR_SCREEN,

} Mirroring;

typedef enum MapperID {
    NROM = 0,
    MMC1,
    UXROM,
    CNROM,
    MMC3,
    AOROM = 7,
    GNROM = 66
} MapperID;

typedef enum MapperFormat {
    ARCHAIC_INES,
    INES,
    NES2,
} MapperFormat;

struct Genie;
struct Emulator;
struct NSF;

typedef struct Mapper{
    uint8_t* CHR_RAM;
    uint8_t* PRG_ROM;
    uint8_t* save_RAM;
    uint8_t* PRG_ptr;
    uint8_t* CHR_ptr;
    uint8_t PRG_banks;
    uint8_t CHR_banks;
    uint8_t RAM_banks;
    Mirroring mirroring;
    TVSystem type;
    MapperFormat format;
    uint16_t name_table_map[4];
    uint32_t clamp;
    uint8_t mapper_num;
    uint8_t is_nsf;
    uint8_t (*read_ROM)(struct Mapper*, uint16_t);
    void (*write_ROM)(struct Mapper*, uint16_t, uint8_t);
    uint8_t (*read_PRG)(struct Mapper*, uint16_t);
    void (*write_PRG)(struct Mapper*, uint16_t, uint8_t);
    uint8_t (*read_CHR)(struct Mapper*, uint16_t);
    void (*write_CHR)(struct Mapper*, uint16_t , uint8_t);
    void (*reset)(struct Mapper*);

    // mapper extension structs would be attached here
    // memory should be allocated dynamically and should
    // not be freed since this is done by the generic mapper functions
    void* extension;
    // pointer to game genie if any
    struct Genie* genie;
    struct NSF* NSF;
    struct Emulator* emulator;
} Mapper;

void load_file(char* file_name, char* game_genie, Mapper* mapper);
void free_mapper(struct Mapper* mapper);
void set_mirroring(Mapper* mapper, Mirroring mirroring);

// mapper specifics

void load_UXROM(Mapper* mapper);
void load_MMC1(Mapper* mapper);
void load_CNROM(Mapper* mapper);
void load_GNROM(Mapper* mapper);
void load_AOROM(Mapper* mapper);
void load_MMC3(Mapper* mapper);