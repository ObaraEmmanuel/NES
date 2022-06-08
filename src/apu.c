#include "apu.h"
#include "emulator.h"
#include "gfx.h"
#include "utils.h"

#define TND_LUT_SIZE 203
#define PULSE_LUT_SIZE 31

static const uint8_t length_counter_lookup[32] = {
// HI/LO 0   1   2   3   4   5   6   7    8   9   A   B   C   D   E   F
// ----------------------------------------------------------------------
/* 0 */  10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
/* 1 */  12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static uint8_t duty[4][8] =
{
    {0, 0, 0, 0, 0, 0, 0, 1 },      // 12.5 %
    {0, 0, 0, 0, 0, 0, 1, 1 },      // 25 %
    {0, 0, 0, 0, 1, 1, 1, 1 },      // 50 %
    {1, 1, 1, 1, 1, 1, 0, 0 }       // 25 % negated
};

static uint8_t tri_sequence[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

static uint16_t noise_period_lookup_NTSC[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

//static uint16_t noise_period_lookup_PAL[16] = {
//    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778
//};

/*
Mode 0: 4-step sequence

Action      Envelopes &     Length Counter& Interrupt   Delay to next
            Linear Counter  Sweep Units     Flag        NTSC     PAL
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
$4017=$00   -               -               -           7459    8315
Step 1      Clock           -               -           7456    8314
Step 2      Clock           Clock           -           7458    8312
Step 3      Clock           -               -           7458    8314
Step 4      Clock           Clock       Set if enabled  7458    8314


Mode 1: 5-step sequence

Action      Envelopes &     Length Counter& Interrupt   Delay to next
            Linear Counter  Sweep Units     Flag        NTSC     PAL
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
$4017=$80   -               -               -              1       1
Step 1      Clock           Clock           -           7458    8314
Step 2      Clock           -               -           7456    8314
Step 3      Clock           Clock           -           7458    8312
Step 4      Clock           -               -           7458    8314
Step 5      -               -               -           7452    8312
 */

static uint16_t length_timing[2][6] = {
    {7459,  7456, 7458, 7458, 7458},
    {1,     7458, 7456, 7458, 7458, 7452}
};


static float tnd_LUT[TND_LUT_SIZE];
static float pulse_LUT[PULSE_LUT_SIZE];

static void compute_mixer_LUT();
static void init_pulse(Pulse* pulse, uint8_t id);
static void init_triangle(Triangle* triangle);
static void init_noise(Noise* noise);
static inline void length_sweep_pulse(Pulse* pulse);

static uint8_t clock_divider(Divider* divider);
static uint8_t clock_triangle(Triangle* triangle);
static uint8_t clock_divider_inverse(Divider* divider);

void init_APU(struct Emulator* emulator){
    bzero(&emulator->apu, sizeof(APU));
    compute_mixer_LUT();
    APU* apu = &emulator->apu;
    apu->emulator = emulator;
    apu->cycles = 0;
    apu->frame.period = length_timing[apu->frame_mode][0];
    apu->frame.from = 1;
    apu->frame.limit = 4;

    apu->sampling.period = 40;
    apu->sampling.counter = apu->sampling.period;
    apu->sampling.from = 0;
    apu->sampling.limit = AUDIO_BUFF_SIZE - 1;
    apu->sampling.step = AUDIO_BUFF_SIZE - 1;

    init_pulse(&apu->pulse1, 1);
    init_pulse(&apu->pulse2, 2);
    init_triangle(&apu->triangle);
    init_noise(&apu->noise);
}

void execute_apu(APU* apu){
    apu->cycles++;
    if(clock_divider(&apu->frame)){
        if(apu->frame.step == 5){
            // do nothing
        }else {
            Triangle* triangle = &apu->triangle;
            //envelope
            clock_divider_inverse(&apu->pulse1.envelope);
            clock_divider_inverse(&apu->pulse2.envelope);
            clock_divider_inverse(&apu->noise.envelope);

            // triangle linear counter
            if(triangle->reload)
                triangle->counter = triangle->r;
            else if(triangle->counter)
                triangle->counter--;
            // if halt is clear, clear reload
            triangle->reload = triangle->halt ? triangle->reload : triangle->halt;

            if ((!(apu->counter_ctrl & BIT_7) && (apu->frame.step == 2 || apu->frame.step == 4)) || ((apu->counter_ctrl & BIT_7) && (apu->frame.step == 1 || apu->frame.step == 3))) {
                // length and sweep
                length_sweep_pulse(&apu->pulse1);
                length_sweep_pulse(&apu->pulse2);
                // triangle length counter
                if(!apu->triangle.halt && apu->triangle.l) {
                    apu->triangle.l--;
                }
                // noise length counter
                Noise* noise = &apu->noise;
                if(noise->l && !noise->envelope.loop)
                    noise->l--;
            }
            if(((apu->frame.step == 4 && !(apu->counter_ctrl & BIT_7)) || apu->frame_interrupt) && !(apu->counter_ctrl & BIT_6)){
                apu->frame_interrupt = 1;
                interrupt(&apu->emulator->cpu, IRQ);
            }

        }
        apu->frame.counter = length_timing[apu->frame_mode][apu->frame.step];
    }
    if(apu->cycles & 1){
        // channel sequencer
        clock_divider(&apu->pulse1.t);
        clock_divider(&apu->pulse2.t);

        // noise timer
        if(clock_divider(&apu->noise.timer)){
            Noise* noise = &apu->noise;
            uint8_t feedback = (noise->shift & BIT_0) ^ (((noise->mode ? BIT_6 : BIT_1) & noise->shift) > 0);
            noise->shift >>= 1;
            noise->shift |= feedback? (1 << 14) : 0;
        }
    }
    // triangle timer
    clock_triangle(&apu->triangle);

    if(clock_divider(&apu->sampling)){
        // printf("%d\n", (int16_t)(32765 * get_sample(apu)));
        apu->buff[apu->sampling.step] = (int16_t)(32765 * get_sample(apu));
    }
}


void queue_audio(APU* apu, struct GraphicsContext* ctx){
    printf("%d\n", apu->sampling.step + 1);
    SDL_QueueAudio(ctx->audio_device, apu->buff, (apu->sampling.step + 1) * 2);
    // printf("queue size %d\n", SDL_GetQueuedAudioSize(ctx->audio_device));
    apu->samples += apu->sampling.step + 1;
    // reset sampler and buffer
    apu->sampling.step = 0;
    apu->sampling.counter = 0;
    bzero(apu->buff, AUDIO_BUFF_SIZE * 2);
}


float get_sample(APU* apu){
    uint8_t pulse_out = 0, tnd_out = 0;

    if(apu->pulse1.enabled)
        pulse_out += (apu->pulse1.const_volume ? apu->pulse1.envelope.period : apu->pulse1.envelope.step) * (!apu->pulse1.mute) * (apu->pulse1.l > 0) * (duty[apu->pulse1.duty][apu->pulse1.t.step]);

    if(apu->pulse2.enabled)
        pulse_out += (apu->pulse2.const_volume ? apu->pulse2.envelope.period : apu->pulse2.envelope.step) * (!apu->pulse2.mute) * (apu->pulse2.l > 0) * (duty[apu->pulse2.duty][apu->pulse2.t.step]);

//    if(apu->triangle.enabled)
//        tnd_out += tri_sequence[apu->triangle.t.step] * 3;
    if(apu->noise.enabled)
        tnd_out += 2 * ((apu->noise.const_volume ? apu->noise.envelope.period : apu->noise.envelope.step) * (apu->noise.shift & BIT_0) * (apu->noise.l > 0));

    float amp = pulse_LUT[pulse_out] + tnd_LUT[tnd_out];

    // clamp to within 1 just in case
    return amp > 1 ? 1 : amp;
}


void set_status(APU* apu, uint8_t value){
    apu->pulse1.enabled = (value & BIT_0) > 0;
    apu->pulse2.enabled = (value & BIT_1) > 0;
    apu->triangle.enabled = (value & BIT_2) > 0;
    apu->noise.enabled = (value & BIT_3) > 0;

    // reset length counters if disabled
    apu->pulse1.l = apu->pulse1.enabled ? apu->pulse1.l : 0;
    apu->pulse2.l = apu->pulse2.enabled ? apu->pulse2.l : 0;
    apu->triangle.l = apu->triangle.enabled ? apu->triangle.l : 0;
    apu->noise.l = apu->noise.enabled ? apu->noise.l : 0;
}


uint8_t read_apu_status(APU* apu){
    uint8_t status = (apu->pulse1.l > 0);
    status |= (apu->pulse2.l > 0 ? BIT_1 : 0);
    status |= (apu->triangle.l > 0 ? BIT_2 : 0);
    status |= (apu->noise.l > 0 ? BIT_3 : 0);
    status |= (apu->frame_interrupt ? BIT_6 : 0);
    // clear frame interrupt
    apu->frame_interrupt = 0;
    return status;
}


void set_frame_counter_ctrl(APU* apu, uint8_t value){
    apu->counter_ctrl = value;
    apu->frame_mode = (value & BIT_7) > 0;
    // clear interrupt if IRQ disable set
    if(value & BIT_6)
        apu->frame_interrupt = 0;
    apu->frame.period = length_timing[apu->frame_mode][1];
    apu->frame.counter = length_timing[apu->frame_mode][0];
    apu->frame.step = 0;
    // either frame_mode 0 -> 4 or 1 -> 5
    apu->frame.limit = apu->frame_mode ? 5 : 4;
}

void set_pulse_ctrl(Pulse* pulse, uint8_t value){
    pulse->const_volume = (value & BIT_4) > 0;
    pulse->envelope.loop = (value & BIT_5) > 0;
    pulse->envelope.period = value & 0xF;
    pulse->envelope.counter = pulse->envelope.period;
    // reload divider step counter
    pulse->envelope.step = 15;
    pulse->duty = value >> 6;
}

void set_pulse_timer(Pulse* pulse, uint8_t value){
    pulse->t.period = value;
}

void set_pulse_sweep(Pulse* pulse, uint8_t value){
    pulse->enable_sweep = (value & BIT_7) > 0;
    pulse->sweep.period = ((value & PULSE_PERIOD) >> 4) + 1;
    pulse->sweep.counter = pulse->sweep.period;
    pulse->shift = value & PULSE_SHIFT;
    pulse->neg = value & BIT_3;
}

void set_pulse_length_counter(Pulse* pulse, uint8_t value){
    pulse->t.period |= ((uint16_t)(value & TIMER_HIGH) << 8);
    pulse->t.period += 1;
    if(pulse->enabled)
        pulse->l = length_counter_lookup[value >> 3];
    pulse->t.step = 0;
    pulse->envelope.step = 15;
}

void set_tri_counter(Triangle* triangle, uint8_t value){
    triangle->r = value & 0x7f;
    triangle->halt = (value & BIT_7) > 0;
}

void set_tri_timer_low(Triangle* triangle, uint8_t value){
    triangle->t.period = value;
}

void set_tri_length(Triangle* triangle, uint8_t value){
    triangle->t.period |= ((value & 0x7) << 8);
    triangle->t.period++;
    triangle->reload = 1;
    if(triangle->enabled)
        triangle->l = length_counter_lookup[value >> 3];
}

void set_noise_ctrl(Noise* noise, uint8_t value){
    noise->const_volume = (value & BIT_4) > 0;
    noise->envelope.loop = (value & BIT_5) > 0;
    noise->envelope.period = value & 0xF;
    noise->envelope.counter = noise->envelope.period;
    noise->envelope.step = 15;
}

void set_noise_period(Noise* noise, uint8_t value){
    noise->timer.period = noise_period_lookup_NTSC[value & 0xF];
    noise->timer.step = noise->timer.period;
    noise->mode = (value & BIT_7) > 0;
}

void set_noise_length(Noise* noise, uint8_t value){
    if(noise->enabled)
        noise->l = length_counter_lookup[value >> 3];
}

static void compute_mixer_LUT(){
    pulse_LUT[0] = 0;
    for(int i = 1; i < PULSE_LUT_SIZE; i++)
        pulse_LUT[i] = 95.52f / (8128.0f / (float)i + 100);
    tnd_LUT[0] = 0;
    for(int i = 1; i < TND_LUT_SIZE; i++)
        tnd_LUT[i] = 163.67f / (24329.0f / (float)i + 100);
}

static void init_pulse(Pulse* pulse, uint8_t id){
    pulse->id = id;
    // start with large period until its set
    pulse->t.step = 0;
    pulse->t.from = 0;
    pulse->t.limit = 7;
    pulse->t.loop = 1;
    pulse->sweep.limit = 0;
    pulse->enabled = 0;
}

static void init_triangle(Triangle* triangle){
    triangle->t.step = 0;
    triangle->t.limit = 31;
    triangle->t.from = 0;
    triangle->enabled = 0;
    triangle->halt = 1;
}

static void init_noise(Noise* noise){
    noise->enabled = 0;
    noise->timer.limit = 0;
    noise->shift = 1;
}

static uint8_t clock_divider(Divider* divider){
    if(divider->counter) {
        divider->counter--;
        return 0;
    } else {
        divider->counter = divider->period;
        divider->step++;
        if(divider->limit && divider->step > divider->limit)
            divider->step = divider->from;
        // trigger clock
        return 1;
    }
}

static uint8_t clock_triangle(Triangle* triangle){
    Divider* divider = &triangle->t;
    if(divider->counter) {
        divider->counter--;
        return 0;
    } else {
        divider->counter = divider->period;
        if(triangle->l && triangle->counter)
            divider->step++;
        if(divider->limit && divider->step > divider->limit)
            divider->step = divider->from;
        // trigger clock
        return 1;
    }
}

static uint8_t clock_divider_inverse(Divider* divider){
    if(divider->counter) {
        divider->counter--;
        return 0;
    } else {
        divider->counter = divider->period;
        if(divider->limit && divider->step == 0 && divider->loop)
            divider->step = divider->limit;
        else if(divider->step)
            divider->step--;
        // trigger clock
        return 1;
    }
}

static inline void length_sweep_pulse(Pulse* pulse){
    // continuously compute target period
    ssize_t target_period = pulse->t.period >> pulse->shift;
    target_period = pulse->neg ? -target_period : target_period;
    // add 1 (2's complement) for pulse 2
    target_period += (pulse->id == 2 ? 1 : 0) + pulse->t.period;
    // clock divider
    uint8_t clock_sweep = clock_divider(&pulse->sweep);
    pulse->mute = 0;
    if(pulse->t.period < 8 || target_period > 0x7ff) {
        // mute the channel
        pulse->mute = 1;
    }
    else if(pulse->enable_sweep && clock_sweep && pulse->shift > 0){
        pulse->t.period = target_period;
    }

    // length counter
    if(pulse->l && !pulse->envelope.loop)
        pulse->l--;

}