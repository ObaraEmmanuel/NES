#include "apu.h"
#include "emulator.h"
#include "gfx.h"
#include "utils.h"

#define TND_LUT_SIZE 203
#define PULSE_LUT_SIZE 31
#define AUDIO_TO_FILE 0

static const uint8_t length_counter_lookup[32] = {
    // HI/LO 0   1   2   3   4   5   6   7    8   9   A   B   C   D   E   F
    // ----------------------------------------------------------------------
    /* 0 */ 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    /* 1 */ 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static uint8_t duty[4][8] =
{
    {0, 1, 0, 0, 0, 0, 0, 1}, // 12.5 %
    {0, 1, 1, 0, 0, 0, 1, 1}, // 25 %
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50 %
    {1, 0, 0, 1, 1, 1, 1, 1} // 25 % negated
};

static uint8_t tri_sequence[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static uint16_t noise_period_lookup_NTSC[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static uint16_t noise_period_lookup_PAL[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
};

/*
Rate   $0   $1   $2   $3   $4   $5   $6   $7   $8   $9   $A   $B   $C   $D   $E   $F
      ------------------------------------------------------------------------------
NTSC  428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
PAL   398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98,  78,  66,  50
*/

static uint16_t dmc_rate_index_NTSC[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
};

static uint16_t dmc_rate_index_PAL[16] = {
    398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98,  78,  66,  50
};


/*
Mode 0: 4-step sequence

Action      Envelopes &     Length Counter& Interrupt   Delay to next
            Linear Counter  Sweep Units     Flag        NTSC     PAL
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
$4017=$00   -               -               -           -       -
Step 1      Clock           -               -           7457    8313
Step 2      Clock           Clock           -           14913   16627
Step 3      Clock           -               -           22371   24939
                                        Set if enabled  29828   33252
Step 4      Clock           Clock       Set if enabled  29829   33253
                                        Set if enabled  0       0

Mode 1: 5-step sequence

Action      Envelopes &     Length Counter& Interrupt   CPU cycle
            Linear Counter  Sweep Units     Flag        NTSC     PAL
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
$4017=$80   -               -               -           -       -
Step 1      Clock           -               -           7457    8313
Step 2      Clock           Clock           -           14913   16627
Step 3      Clock           -               -           22371   24939
Step 4      -               -               -           29829   33253
Step 5      Clock           Clock           -           37281   41565
            -               -               -           0       0
 */


static float tnd_LUT[TND_LUT_SIZE];
static float pulse_LUT[PULSE_LUT_SIZE];

static void compute_mixer_LUT();

static void init_audio_device(const APU* apu);

static void init_pulse(Pulse *pulse, uint8_t id);

static void init_triangle(Triangle *triangle);

static void init_noise(Noise *noise);

static void init_dmc(DMC* dmc);

static void init_sampler(APU* apu, int frequency);

static void length_sweep_pulse(Pulse *pulse);

static uint8_t clock_divider(Divider *divider);

static uint8_t clock_triangle(Triangle *triangle);

static uint8_t clock_divider_inverse(Divider *divider);

static void clock_dmc(APU* apu);

static void quarter_frame(APU *apu);

static void half_frame(APU *apu);

static void sample(APU* apu);

FILE *out_wav;

void init_APU(struct Emulator *emulator) {
    memset(&emulator->apu, 0, sizeof(APU));
    compute_mixer_LUT();
    APU *apu = &emulator->apu;
    apu->emulator = emulator;
    apu->cycles = 0;
    apu->sequencer = 0;
    apu->reset_sequencer = 0;
    apu->audio_start = 0;
    apu->IRQ_inhibit = 0;

    // For keeping track of queue_size statistics for use by the adaptive sampler
    memset(apu->stat_window, 0, sizeof(apu->stat_window));
    apu->stat = 0;
    apu->stat_index = 0;

    init_pulse(&apu->pulse1, 1);
    init_pulse(&apu->pulse2, 2);
    init_triangle(&apu->triangle);
    init_noise(&apu->noise);
    init_dmc(&apu->dmc);
    init_sampler(apu, SAMPLING_FREQUENCY);
    init_audio_device(apu);
    SDL_PauseAudioDevice(emulator->g_ctx.audio_device, 1);
    set_status(apu, 0);
#if AUDIO_TO_FILE
    out_wav = fopen("test-aud.raw", "wb");
#endif
}

void init_audio_device(const APU* apu) {
    SDL_AudioSpec want;
    SDL_zero(want);
    /* Set the audio format */
    want.freq = SAMPLING_FREQUENCY;
    want.format = AUDIO_S16SYS;
    want.channels = 1;    /* 1 = mono, 2 = stereo */
    // want.samples = 1024;  /* Good low-latency value for callback */
    want.callback = NULL;
    want.userdata = NULL;
    want.silence = 0;

    apu->emulator->g_ctx.audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (apu->emulator->g_ctx.audio_device == 0) {
        LOG(ERROR , SDL_GetError());
        exit(EXIT_FAILURE);
    }
}

void exit_APU() {
    if (out_wav)
        fclose(out_wav);
}

void execute_apu(APU *apu) {
    // Perform necessary reset after $4017 write
    if (apu->reset_sequencer) {
        apu->reset_sequencer = 0;
        if (apu->frame_mode == 1) {
            quarter_frame(apu);
            half_frame(apu);
        }
        apu->sequencer = 0;
        goto post_sequencer;
    }

    switch (apu->emulator->type) {
    case NTSC:
    default:
        switch (apu->sequencer) {
            case 0:
                apu->sequencer++;
                break;
            case 7457:
                quarter_frame(apu);
                apu->sequencer++;
                break;
            case 14913:
                quarter_frame(apu);
                half_frame(apu);
                apu->sequencer++;
                break;
            case 22371:
                quarter_frame(apu);
                apu->sequencer++;
                break;
            case 29828:
                apu->sequencer++;
                break;
            case 29829:
                if (apu->frame_mode == 1) {
                    apu->sequencer++;
                    break;
                }

                quarter_frame(apu);
                half_frame(apu);

                if (!apu->IRQ_inhibit) {
                    apu->frame_interrupt = 1;
                    interrupt(&apu->emulator->cpu, IRQ);
                }
                apu->sequencer = 0;
                break;
            case 37281:
                quarter_frame(apu);
                half_frame(apu);
                apu->sequencer = 0;
                break;
            default:
                apu->sequencer++;
        }
        break;
    case PAL:
        // code looks repetitive but if you can do it better while still being fast be my guest
        switch (apu->sequencer) {
            case 0:
                apu->sequencer++;
                break;
            case 8313:
                quarter_frame(apu);
                apu->sequencer++;
                break;
            case 16627:
                quarter_frame(apu);
                half_frame(apu);
                apu->sequencer++;
                break;
            case 24939:
                quarter_frame(apu);
                apu->sequencer++;
                break;
            case 33252:
                apu->sequencer++;
                break;
            case 33253:
                if (apu->frame_mode == 1) {
                    apu->sequencer++;
                    break;
                }

                quarter_frame(apu);
                half_frame(apu);

                if (!apu->IRQ_inhibit) {
                    apu->frame_interrupt = 1;
                    interrupt(&apu->emulator->cpu, IRQ);
                }
                apu->sequencer = 0;
                break;
            case 41565:
                quarter_frame(apu);
                half_frame(apu);
                apu->sequencer = 0;
                break;
            default:
                apu->sequencer++;
        }
        break;
    }

post_sequencer:

    if (apu->cycles & 1) {
        // channel sequencer
        clock_divider(&apu->pulse1.t);
        clock_divider(&apu->pulse2.t);

        // noise timer
        if (clock_divider(&apu->noise.timer)) {
            Noise *noise = &apu->noise;
            uint8_t feedback = (noise->shift & BIT_0) ^ (((noise->mode ? BIT_6 : BIT_1) & noise->shift) > 0);
            noise->shift >>= 1;
            noise->shift |= feedback ? (1 << 14) : 0;
        }
    }

    // DMC
    clock_dmc(apu);

    // triangle timer
    clock_triangle(&apu->triangle);

    // sample
    sample(apu);

    apu->cycles++;
}

void quarter_frame(APU *apu) {
    Triangle *triangle = &apu->triangle;
    //envelope
    clock_divider_inverse(&apu->pulse1.envelope);
    clock_divider_inverse(&apu->pulse2.envelope);
    clock_divider_inverse(&apu->noise.envelope);

    // triangle linear counter
    if (triangle->linear_reload_flag)
        triangle->linear_counter = triangle->linear_reload;
    else if (triangle->linear_counter)
        triangle->linear_counter--;
    // if halt is clear, clear linear reload flag
    triangle->linear_reload_flag = triangle->halt ? triangle->linear_reload_flag : 0;
}

void half_frame(APU *apu) {
    // length and sweep
    length_sweep_pulse(&apu->pulse1);
    length_sweep_pulse(&apu->pulse2);
    // triangle length counter
    if (!apu->triangle.halt && apu->triangle.length_counter) {
        apu->triangle.length_counter--;
    }
    // noise length counter
    Noise *noise = &apu->noise;
    if (noise->l && !noise->envelope.loop)
        noise->l--;
}

void init_sampler(APU* apu, int frequency) {
    float cycles_per_frame = apu->emulator->type == PAL? 33247.5: 29780.5;
    Sampler* sampler = &apu->sampler;

    sampler->max_period = cycles_per_frame * 60.0f / frequency;
    sampler->min_period = sampler->max_period - 1;
    sampler->period = sampler->min_period;
    sampler->index = 0;
    sampler->max_index = AUDIO_BUFF_SIZE;
    sampler->samples = 0;
    sampler->counter = 0;
    sampler->factor_index = 0;
    // basically the precision with which we vary the sampling rate
    // 100 ->2 d.p, 1000->3 d.p, etc.
    sampler->max_factor = 100;
    // this may need to be calibrated to suit the current sampling frequency
    // the current equilibrium is for 48000 hz
    sampler->target_factor = sampler->equilibrium_factor = 48;
}


void sample(APU* apu) {
#if AVERAGE_DOWNSAMPLING
    static float avg = -1;
    // average samples in a bin
    if(avg < 0)
        avg = get_sample(apu);
    else
        avg = (avg + get_sample(apu))/2;
#endif

    Sampler* sampler = &apu->sampler;
    sampler->counter++;
    if(sampler->counter >= sampler->period) {
#if AVERAGE_DOWNSAMPLING
        apu->buff[sampler->index++] = (int16_t) (32767 * avg);
        // begin fresh average for the next bin
        avg = -1;
#else
        apu->buff[sampler->index++] = (int16_t) (32767 * get_sample(apu));
#endif
        if(sampler->index >= sampler->max_index) {
            sampler->index = 0;
        }
        sampler->samples++;
        sampler->counter = 0;
        if(apu->sampler.factor_index <= apu->sampler.target_factor) {
            sampler->period = sampler->max_period;
        }else {
            sampler->period = sampler->min_period;
        }
        sampler->factor_index++;
        if(sampler->factor_index > sampler->max_factor) {
            sampler->factor_index = 0;
        }
    }
}


void queue_audio(APU *apu, struct GraphicsContext *ctx) {
    uint32_t queue_size = SDL_GetQueuedAudioSize(ctx->audio_device);
    apu->stat = apu->stat - apu->stat_window[apu->stat_index] + queue_size;
    apu->stat_window[apu->stat_index++] = queue_size;
    if(apu->stat_index >= STATS_WIN_SIZE)
        apu->stat_index = 0;

    size_t avg = apu->stat / STATS_WIN_SIZE;
    // printf("queue size %d, avg: %llu \n", queue_size, avg);

    // From here we tweak the sampling rate ever so slightly to prevent underruns and runaway latency
    // by minimising deviation from the nominal queue size with a bit of control engineering
    float delta_f, error = (float)avg - NOMINAL_QUEUE_SIZE;
    Sampler* s = &apu->sampler;
    if(error >= 0) {
        delta_f = (s->max_factor - s->equilibrium_factor) * error / NOMINAL_QUEUE_SIZE;
    }else {
        delta_f = (s->equilibrium_factor * error / NOMINAL_QUEUE_SIZE);
    }
    // printf("delta %f, error %f \n", delta_f, error);
    s->target_factor = s->equilibrium_factor + delta_f;
    if(s->target_factor > s->max_factor) {
        s->target_factor = s->max_factor;
    }
    // printf("target_f %d \n", s->target_factor);

    SDL_QueueAudio(ctx->audio_device, apu->buff, s->index * 2);
    // wait till queue is filled to prevent early onset underruns
    if(!apu->audio_start && queue_size >= NOMINAL_QUEUE_SIZE) {
        SDL_PauseAudioDevice(apu->emulator->g_ctx.audio_device, 0);
        apu->audio_start = 1;
    }
#if AUDIO_TO_FILE
    if(out_wav)
        fwrite(apu->buff, 2, s->index, out_wav);
#endif
    memset(apu->buff, 0, s->index * 2);
    // reset sampler
    s->index = 0;
}


float get_sample(APU *apu) {
    uint8_t pulse_out = 0, tnd_out = 0;

    if (apu->pulse1.enabled && apu->pulse1.l && !apu->pulse1.mute)
        pulse_out += (apu->pulse1.const_volume ? apu->pulse1.envelope.period : apu->pulse1.envelope.step) * (duty[apu->
            pulse1.duty][apu->pulse1.t.step]);

    if (apu->pulse2.enabled && apu->pulse2.l && !apu->pulse2.mute)
        pulse_out += (apu->pulse2.const_volume ? apu->pulse2.envelope.period : apu->pulse2.envelope.step) * (duty[apu->
            pulse2.duty][apu->pulse2.t.step]);

    if (apu->triangle.enabled)
        tnd_out += tri_sequence[apu->triangle.sequencer.step] * 3;

    if (apu->noise.enabled)
        tnd_out += 2 * ((apu->noise.const_volume ? apu->noise.envelope.period : apu->noise.envelope.step) * (
                            apu->noise.shift & BIT_0) * (apu->noise.l > 0));

    tnd_out += apu->dmc.counter;

    float amp = pulse_LUT[pulse_out] + tnd_LUT[tnd_out];

    // clamp to within 1 just in case
    return amp > 1 ? 1 : amp;
}


void set_status(APU *apu, uint8_t value) {
    apu->pulse1.enabled = (value & BIT_0) > 0;
    apu->pulse2.enabled = (value & BIT_1) > 0;
    apu->triangle.enabled = (value & BIT_2) > 0;
    apu->noise.enabled = (value & BIT_3) > 0;
    apu->dmc.enabled = (value & BIT_4) > 0;

    if(apu->dmc.enabled && apu->dmc.bytes_remaining == 0) {
        // restart it
        apu->dmc.bytes_remaining = apu->dmc.sample_length;
        apu->dmc.current_addr = apu->dmc.sample_addr;
    }else if(!apu->dmc.enabled) {
        apu->dmc.bytes_remaining = 0;
    }
    apu->dmc.interrupt = 0;


    // reset length counters if disabled
    apu->pulse1.l = apu->pulse1.enabled ? apu->pulse1.l : 0;
    apu->pulse2.l = apu->pulse2.enabled ? apu->pulse2.l : 0;
    apu->triangle.length_counter = apu->triangle.enabled ? apu->triangle.length_counter : 0;
    apu->noise.l = apu->noise.enabled ? apu->noise.l : 0;
}


uint8_t read_apu_status(APU *apu) {
    uint8_t status = (apu->pulse1.l > 0);
    status |= (apu->pulse2.l > 0 ? BIT_1 : 0);
    status |= (apu->triangle.length_counter > 0 ? BIT_2 : 0);
    status |= (apu->noise.l > 0 ? BIT_3 : 0);
    status |= (apu->frame_interrupt ? BIT_6 : 0);
    status |= (apu->dmc.interrupt ? BIT_7 : 0);
    status |= (apu->dmc.bytes_remaining? BIT_4: 0);
    // clear frame interrupt
    apu->frame_interrupt = 0;
    return status;
}


void set_frame_counter_ctrl(APU *apu, uint8_t value) {
    // $4017
    apu->IRQ_inhibit = (value & BIT_6) > 0;
    apu->frame_mode = (value & BIT_7) > 0;
    // clear interrupt if IRQ disable set
    if (value & BIT_6)
        apu->frame_interrupt = 0;
    apu->reset_sequencer = 1;
}

void set_pulse_ctrl(Pulse *pulse, uint8_t value) {
    pulse->const_volume = (value & BIT_4) > 0;
    pulse->envelope.loop = (value & BIT_5) > 0;
    pulse->envelope.period = value & 0xF;
    pulse->envelope.counter = pulse->envelope.period;
    // reload divider step counter
    pulse->envelope.step = 15;
    pulse->duty = value >> 6;
}

void set_pulse_timer(Pulse *pulse, uint8_t value) {
    pulse->t.period = pulse->t.period & ~0xff | value;
}

void set_pulse_sweep(Pulse *pulse, uint8_t value) {
    pulse->enable_sweep = (value & BIT_7) > 0;
    pulse->sweep.period = ((value & PULSE_PERIOD) >> 4) + 1;
    pulse->sweep.counter = pulse->sweep.period;
    pulse->shift = value & PULSE_SHIFT;
    pulse->neg = value & BIT_3;
}

void set_pulse_length_counter(Pulse *pulse, uint8_t value) {
    pulse->t.period = pulse->t.period & 0xff | (value & 0x7) << 8;
    if (pulse->enabled)
        pulse->l = length_counter_lookup[value >> 3];
    pulse->t.step = 0;
    pulse->envelope.step = 15;
}

void set_tri_counter(Triangle *triangle, uint8_t value) {
    triangle->linear_reload = value & 0x7f;
    triangle->halt = (value & BIT_7) > 0;
}

void set_tri_timer_low(Triangle *triangle, uint8_t value) {
    triangle->sequencer.period = triangle->sequencer.period & ~0xff | value;
}

void set_tri_length(Triangle *triangle, uint8_t value) {
    triangle->sequencer.period = triangle->sequencer.period & 0xff | (value & 0x7) << 8;
    triangle->linear_reload_flag = 1;
    if (triangle->enabled)
        triangle->length_counter = length_counter_lookup[value >> 3];
}

void set_noise_ctrl(Noise *noise, uint8_t value) {
    noise->const_volume = (value & BIT_4) > 0;
    noise->envelope.loop = (value & BIT_5) > 0;
    noise->envelope.period = value & 0xF;
    noise->envelope.counter = noise->envelope.period;
    noise->envelope.step = 15;
}

void set_noise_period(APU* apu, uint8_t value) {
    Noise* noise = &apu->noise;
    if(apu->emulator->type == PAL)
        noise->timer.period = noise_period_lookup_PAL[value & 0xF];
    else
        noise->timer.period = noise_period_lookup_NTSC[value & 0xF];
    noise->timer.step = noise->timer.period;
    noise->mode = (value & BIT_7) > 0;
}

void set_noise_length(Noise *noise, uint8_t value) {
    if (noise->enabled)
        noise->l = length_counter_lookup[value >> 3];
}

void set_dmc_ctrl(APU* apu, uint8_t value) {
    apu->dmc.loop = (value & BIT_6) > 0;
    apu->dmc.IRQ_enable = (value & BIT_7) > 0;
    if(!apu->dmc.IRQ_enable) {
        apu->dmc.interrupt = 0;
    }
    if(apu->emulator->type == NTSC)
        apu->dmc.rate = dmc_rate_index_NTSC[value & 0xf] - 1;
    else
        apu->dmc.rate = dmc_rate_index_PAL[value & 0xf] - 1;
}

void set_dmc_da(DMC* dmc, uint8_t value) {
    dmc->counter = value & 0x7F;
}

void set_dmc_addr(DMC* dmc, uint8_t value) {
    dmc->sample_addr = 0xC000 + (uint16_t)value * 64;
}

void set_dmc_length(DMC* dmc, uint8_t value) {
    dmc->sample_length = (uint16_t)value * 16 + 1;
}

void clock_dmc(APU* apu) {
    DMC* dmc = &apu->dmc;

    if(dmc->enabled && dmc->empty) {
        if(dmc->bytes_remaining > 0) {
            apu->emulator->cpu.dma_cycles += 3;
            dmc->sample = read_mem(&apu->emulator->mem, dmc->current_addr);
            dmc->empty = 0;
            dmc->bytes_remaining--;
            if(dmc->current_addr == 0xffff)
                dmc->current_addr = 0x8000;
            else
                dmc->current_addr++;
            dmc->irq_set = 0;
        }
        if(dmc->bytes_remaining == 0) {
            if(dmc->loop) {
                dmc->current_addr = dmc->sample_addr;
                dmc->bytes_remaining = dmc->sample_length;
            }else if(dmc->IRQ_enable && !dmc->irq_set) {
                dmc->interrupt = 1;
                dmc->irq_set = 1;
                interrupt(&apu->emulator->cpu, IRQ);
            }
        }
    }

    if(dmc->rate_index > 0) {
        dmc->rate_index--;
        return;
    }
    dmc->rate_index = dmc->rate;

    if(dmc->bits_remaining > 0) {
        // clamped counter update
        if(!dmc->silence) {
            if(dmc->bits & 1) {
                dmc->counter+=2;
                dmc->counter = dmc->counter > 127 ? 127 : dmc->counter;
            }
            else if(dmc->counter > 1)
                dmc->counter-=2;
            dmc->bits >>= 1;
        }
        dmc->bits_remaining--;
    }
    if(dmc->bits_remaining == 0) {
        if(dmc->empty)
            dmc->silence = 1;
        else {
            dmc->bits = dmc->sample;
            dmc->empty = 1;
            dmc->silence = 0;
        }
        dmc->bits_remaining = 8;
    }
}

static void compute_mixer_LUT() {
    pulse_LUT[0] = 0;
    for (int i = 1; i < PULSE_LUT_SIZE; i++)
        pulse_LUT[i] = 95.52f / (8128.0f / (float) i + 100);
    tnd_LUT[0] = 0;
    for (int i = 1; i < TND_LUT_SIZE; i++)
        tnd_LUT[i] = 163.67f / (24329.0f / (float) i + 100);
}

static void init_pulse(Pulse *pulse, uint8_t id) {
    pulse->id = id;
    // start with large period until its set
    pulse->t.step = 0;
    pulse->t.from = 0;
    pulse->t.limit = 7;
    pulse->t.loop = 1;
    pulse->sweep.limit = 0;
    pulse->enabled = 0;
}

static void init_triangle(Triangle *triangle) {
    triangle->sequencer.step = 0;
    triangle->sequencer.limit = 31;
    triangle->sequencer.from = 0;
    triangle->enabled = 0;
    triangle->halt = 1;
}

static void init_noise(Noise *noise) {
    noise->enabled = 0;
    noise->timer.limit = 0;
    noise->shift = 1;
}

static void init_dmc(DMC* dmc) {
    dmc->empty = 1;
    dmc->silence = 1;
}

static uint8_t clock_divider(Divider *divider) {
    if (divider->counter) {
        divider->counter--;
        return 0;
    }

    divider->counter = divider->period;
    divider->step++;
    if (divider->limit && divider->step > divider->limit)
        divider->step = divider->from;
    // trigger clock
    return 1;
}

static uint8_t clock_triangle(Triangle *triangle) {
    Divider *divider = &triangle->sequencer;
    if (divider->counter) {
        divider->counter--;
        return 0;
    }

    divider->counter = divider->period;
    if (triangle->length_counter && triangle->linear_counter)
        divider->step++;
    if (divider->limit && divider->step > divider->limit)
        divider->step = divider->from;
    // trigger clock
    return 1;
}

static uint8_t clock_divider_inverse(Divider *divider) {
    if (divider->counter) {
        divider->counter--;
        return 0;
    }
    divider->counter = divider->period;
    if (divider->limit && divider->step == 0 && divider->loop)
        divider->step = divider->limit;
    else if (divider->step)
        divider->step--;
    // trigger clock
    return 1;
}

static void length_sweep_pulse(Pulse *pulse) {
    // continuously compute target period
    long long target_period = pulse->t.period >> pulse->shift;
    target_period = pulse->neg ? -target_period : target_period;
    // add 1 (2's complement) for pulse 2
    target_period += (pulse->id == 2 ? 1 : 0) + pulse->t.period;
    // clock divider
    uint8_t clock_sweep = clock_divider(&pulse->sweep);
    pulse->mute = 0;
    if (pulse->t.period < 8 || target_period > 0x7ff) {
        // mute the channel
        pulse->mute = 1;
    } else if (pulse->enable_sweep && clock_sweep && pulse->shift > 0) {
        pulse->t.period = target_period;
    }

    // length counter
    if (pulse->l && !pulse->envelope.loop)
        pulse->l--;
}
