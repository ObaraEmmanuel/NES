# pragma once

#include "gfx.h"
#include "mapper.h"
#include "apu.h"
#include "utils.h"
#include <SDL.h>

#define NSF_HEADER_SIZE 0x80
#define TEXT_FIELD_SIZE 32
// Should be greater than TEXT_FIELD_SIZE
#define MAX_TEXT_FIELD_SIZE 40
#define MAX_TRACK_NAME_SIZE 24

#define NSF_DEFAULT_TRACK_DUR 180000 // ms

typedef enum NSFFormat{
    NSF1 = 1,
    NSF2 = 2
}NSFFormat;

typedef enum NSFFlags {
    NSF_IRQ                 = BIT_4,
    NSF_NON_RETURN_INIT     = BIT_5,
    NSF_NO_PLAY_SR          = BIT_6,
    NSF_REQ_NSFE_CHUNKS     = BIT_7,
}NSFFlags;

typedef struct NSF {
    uint8_t version;
    uint8_t flags;
    uint8_t total_songs;
    uint8_t starting_song;
    uint8_t current_song;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    // --- NSF2 specific ---
    uint16_t IRQ_vector;
    uint16_t IRQ_counter;
    uint16_t IRQ_counter_reload;
    uint8_t IRQ_status;
    uint8_t init_num;
    // ---------------------
    char song_name[MAX_TEXT_FIELD_SIZE+1];
    char artist[MAX_TEXT_FIELD_SIZE+1];
    char copyright[MAX_TEXT_FIELD_SIZE+1];
    char ripper[MAX_TEXT_FIELD_SIZE+1];
    char** tlbls;
    int* times;
    int* fade;
    double tick;
    int tick_max;
    uint16_t speed;
    uint8_t bank_switch;
    uint8_t* bank_ptrs[8];
    uint8_t bank_init[8];
    uint8_t initializing;
    size_t prg_size;

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
} NSF;

void load_nsf(SDL_IOStream* file, Mapper* mapper);
void load_nsfe(SDL_IOStream* file, Mapper* mapper);
void free_NSF(NSF* nsf);
void next_song(struct Emulator* emulator, NSF* nsf);
void prev_song(struct Emulator* emulator, NSF* nsf);
void init_song(struct Emulator* emulator, size_t song_number);
void nsf_execute(struct Emulator* emulator);
void init_NSF_gfx(GraphicsContext* g_ctx, NSF* nsf);
void render_NSF_graphics(struct Emulator* emulator, NSF* nsf);
