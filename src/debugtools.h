#pragma once

#include <stdint.h>

struct PPU;

void render_name_tables(struct PPU* ppu, uint32_t* screen);
