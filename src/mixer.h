#pragma once

#include "apu.h"
#include "biquad.h"

#include <SDL.h>

#define SAMPLING_FREQUENCY 48000
// should be able to store samples produced in 1/60th of a second
// for the target sampling frequency
// higher sampling frequency will need a bigger buffer
#define AVERAGE_DOWNSAMPLING 0
#define NOMINAL_QUEUE_SIZE 6000

#if DISABLE_AUDIO
// no need to allocate so much memory that is not going to be used
#define AUDIO_BUFF_SIZE 1
#define STATS_WIN_SIZE 1
#else
#define AUDIO_BUFF_SIZE 1024
#define STATS_WIN_SIZE 20
#endif


struct Emulator;

typedef struct {
    uint16_t factor_index;
    uint16_t target_factor;
    uint16_t equilibrium_factor;
    uint16_t max_factor;
    size_t samples;
    size_t max_period;
    size_t min_period;
    size_t period;
    size_t counter;
    size_t index;
    size_t max_index;
} Sampler;

void init_mixer(struct Emulator* emulator);

typedef struct {
    int16_t buff[AUDIO_BUFF_SIZE];
    size_t stat_window[STATS_WIN_SIZE];
    Sampler sampler;
    float volume;
    uint8_t audio_start;
#if DISABLE_AUDIO == 0
    SDL_AudioStream* audio_stream;
    float stat;
    size_t stat_index;
    Biquad filter;
    Biquad aa_filter;
    APU* apu;
#endif
} Mixer;

void sample(Mixer* mixer);
void queue_audio(Mixer* mixer);
void pause_audio(Mixer* mixer, int flag);
void free_mixer(const Mixer* mixer);

