#pragma once

#include <SDL.h>
#include "utils.h"
#include "apu.h"
#include "emulator.h"

typedef struct NSFGraphicsContext {
    SDL_Texture* song_info_tx;
    SDL_FRect song_info_rect;
    SDL_Texture* song_num_tx;
    SDL_FRect song_num_rect;
    SDL_Texture* song_dur_tx;
    SDL_FRect song_dur_rect;
    SDL_Texture* song_dur_max_tx;
    SDL_FRect song_dur_max_rect;
    complx samples[AUDIO_BUFF_SIZE];
    complx temp[AUDIO_BUFF_SIZE];

    Emulator* emulator;
} NSFGraphicsContext;

void render_NSF_graphics(NSFGraphicsContext* nsf_ctx);
void init_NSF_graphics(Emulator* emulator, NSFGraphicsContext* nsf_ctx);
void free_NSF_graphics(NSFGraphicsContext* nsf_ctx);