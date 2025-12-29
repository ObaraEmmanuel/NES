#pragma once

#include <stdint.h>

#include "mapper.h"

typedef struct Genie{
    Mapper g_mapper;
    Mapper* mapper;
    uint16_t address1, address2, address3;
    uint8_t cmp1, cmp2, cmp3, repl1, repl2, repl3, ctrl;
} Genie;

int load_genie(ROMData* rom_data, Mapper* mapper);