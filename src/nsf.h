# pragma once

#include "gfx.h"
#include "mapper.h"
#include "apu.h"
#include "utils.h"
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL.h>

#define NSF_HEADER_SIZE 0x80
#define TEXT_FIELD_SIZE 32

#define NSF_SENTINEL_ADDR 0x5FF5

typedef enum NSFFormat{
    NSFE = 1,
    NSF2 = 2
}NSFFormat;

typedef struct NSF {
    uint8_t version;
    uint8_t total_songs;
    uint8_t starting_song;
    uint8_t current_song;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    char song_name[TEXT_FIELD_SIZE];
    char artist[TEXT_FIELD_SIZE];
    char copyright[TEXT_FIELD_SIZE];
    uint16_t speed;
    uint8_t bank_switch;
    uint8_t* bank_ptrs[8];
    uint8_t bank_init[8];
    uint8_t initializing;
    size_t prg_size;

    SDL_Texture* song_info_tx;
    SDL_Rect song_info_rect;
    SDL_Texture* song_num_tx;
    SDL_Rect song_num_rect;
    complx samples[AUDIO_BUFF_SIZE];
    complx temp[AUDIO_BUFF_SIZE];
} NSF;

void load_nsf(SDL_RWops* file, Mapper* mapper);
void init_song(struct Emulator* emulator, size_t song_number);
void nsf_jsr(struct Emulator* emulator, uint16_t address);
void init_NSF_gfx(GraphicsContext* g_ctx, NSF* nsf);
void render_NSF_graphics(struct Emulator* emulator, NSF* nsf);
