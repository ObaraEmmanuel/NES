#pragma once

#include <stdint.h>
#include <stdio.h>

typedef enum{
    VERTICAL,
    HORIZONTAL,
    ONE_SCREEN,
    ONE_SCREEN_LOWER,
    ONE_SCREEN_UPPER,
    FOUR_SCREEN,

} Mirroring;

typedef enum{
    NROM = 0,
    MMC1,
    UXROM,
    CNROM,
    MMC3
} MapperID;

typedef struct Mapper{
    uint8_t* CHR_RAM;
    uint8_t* PRG_ROM;
    uint8_t* save_RAM;
    uint8_t PRG_banks;
    uint8_t CHR_banks;
    uint8_t RAM_banks;
    uint8_t bank_select;
    Mirroring mirroring;
    uint16_t name_table_map[4];
    uint8_t mapper_num;
    uint8_t (*read_PRG)(struct Mapper*, uint16_t);
    void (*write_PRG)(struct Mapper*, uint16_t, uint8_t);
    uint8_t (*read_CHR)(struct Mapper*, uint16_t);
    void (*write_CHR)(struct Mapper*, uint16_t , uint8_t);

    // mapper extension structs would be attached here
    // memory should be allocated dynamically and should
    // not be freed since this is done by the generic mapper functions
    void* extension;
} Mapper;

void load_file(char* file_name, Mapper* mapper);
void free_mapper(struct Mapper* mapper);
void set_mirroring(Mapper* mapper, Mirroring mirroring);

// mapper specifics

void load_UXROM(Mapper* mapper);
void load_MMC1(Mapper* mapper);
void load_CNROM(Mapper* mapper);