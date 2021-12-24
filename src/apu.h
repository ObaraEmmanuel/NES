#pragma once

#include <stdint.h>
#include <stdlib.h>

#define AUDIO_BUFF_SIZE 728

struct Emulator;
struct GraphicsContext;

enum {
    TIMER_HIGH = 0x7,
    PULSE_SHIFT = 0x7,
    PULSE_PERIOD = 0b01110000,
};

typedef struct {
    ssize_t period;
    ssize_t counter;
    uint32_t step;
    uint32_t limit;
    uint32_t from;
    uint8_t loop;
} Divider;

typedef struct {
    Divider t;     // 11 bit timer
    uint8_t l;      // length counter
    uint8_t id;     // pulse id whether 1 or 2
    uint8_t neg;
    uint8_t shift;
    Divider sweep;
    uint8_t enable_sweep;
    uint8_t duty;
    uint8_t const_volume;
    Divider envelope;
    uint8_t envelope_loop;
    uint8_t enabled;
    uint8_t mute;
} Pulse;


typedef struct {
    Divider t;
    uint8_t l;
    uint8_t r;
    uint8_t counter;
    uint8_t reload;
    uint8_t halt;
    uint8_t enabled;
} Triangle;


typedef struct {
    Divider timer;
    uint8_t mode;
    uint8_t l;
    uint16_t shift;
    uint8_t const_volume;
    Divider envelope;
    uint8_t envelope_loop;
    uint8_t enabled;
} Noise;


typedef struct APU{
    struct Emulator* emulator;
    uint16_t buff[AUDIO_BUFF_SIZE];
    Divider frame;
    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;
    uint8_t frame_mode;
    uint8_t status;
    uint8_t counter_ctrl;
    uint8_t frame_interrupt;
    size_t cycles;
    Divider sampling;
    size_t samples;
} APU;


void init_APU(struct Emulator* emulator);
void execute_apu(APU* apu);
void set_status(APU* apu, uint8_t value);
float get_sample(APU* apu);
void queue_audio(APU* apu, struct GraphicsContext* ctx);
uint8_t read_apu_status(APU* apu);
void set_frame_counter_ctrl(APU* apu, uint8_t value);

void set_pulse_ctrl(Pulse* pulse, uint8_t value);
void set_pulse_timer(Pulse* pulse, uint8_t value);
void set_pulse_sweep(Pulse* pulse, uint8_t value);
void set_pulse_length_counter(Pulse* pulse, uint8_t value);

void set_tri_counter(Triangle* triangle, uint8_t value);
void set_tri_timer_low(Triangle* triangle, uint8_t value);
void set_tri_length(Triangle* triangle, uint8_t value);

void set_noise_ctrl(Noise* noise, uint8_t value);
void set_noise_period(Noise* noise, uint8_t value);
void set_noise_length(Noise* noise, uint8_t value);