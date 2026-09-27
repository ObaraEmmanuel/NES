#include "mixer.h"
#include "emulator.h"
#include "utils.h"

#include <string.h>

#if DISABLE_AUDIO

void init_mixer(Emulator* emulator){}
void sample(Mixer* mixer){}
void queue_audio(Mixer* mixer){}
void pause_audio(Mixer* mixer, int flag){}
void free_mixer(const Mixer* mixer){}

#else

static void init_sampler(Mixer* mixer, int frequency, TVSystem sys);
static void init_audio_device(Mixer* mixer);

void init_mixer(Emulator* emulator) {
    Mixer* mixer = &emulator->mixer;
    mixer->apu = &emulator->apu;
    // For keeping track of queue_size statistics for use by the adaptive sampler
    memset(mixer->stat_window, 0, sizeof(mixer->stat_window));
    mixer->stat = 0;
    mixer->stat_index = 0;
    mixer->audio_start = 0;
    mixer->volume = 1;

    init_sampler(mixer, SAMPLING_FREQUENCY, emulator->type);
    init_audio_device(mixer);
    pause_audio(mixer, 1);
}

void init_audio_device(Mixer* mixer) {

    const SDL_AudioSpec spec = {
        .format = SDL_AUDIO_S16,
        .channels = 1,
        .freq = SAMPLING_FREQUENCY
    };

    mixer->audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (mixer->audio_stream == NULL) {
        LOG(ERROR , SDL_GetError());
        quit(EXIT_FAILURE);
    }
}

void init_sampler(Mixer* mixer, int frequency, TVSystem sys) {
    float cycles_per_frame = sys == PAL? 33247.5: 29780.5;
    float rate = sys == PAL? 50.0f : 60.0f;
    Sampler* sampler = &mixer->sampler;
    // Q = 0.707 => BW = 1.414 (1 octave)
    biquad_init(&mixer->filter, HPF, 0, 20, frequency, 1);
    // anti-aliasing filter.
    biquad_init(&mixer->aa_filter, LPF, 0, 20000, cycles_per_frame * rate, 1);

    sampler->max_period = cycles_per_frame * rate / frequency;
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

void sample(Mixer* mixer) {
    float sample = biquad(get_sample(mixer->apu), &mixer->aa_filter);
#if AVERAGE_DOWNSAMPLING
    static float avg = -1;
    // average samples in a bin
    if(avg < 0)
        avg = sample;
    else
        avg = (avg + sample)/2;
#endif

    Sampler* sampler = &mixer->sampler;
    sampler->counter++;
    if(sampler->counter >= sampler->period) {
#if AVERAGE_DOWNSAMPLING
        apu->buff[sampler->index++] = 32767 * biquad(avg, &apu->filter);
        // begin fresh average for the next bin
        avg = -1;
#else

        mixer->buff[sampler->index++] = 32000 * biquad(sample, &mixer->filter) * mixer->volume;
#endif
        if(sampler->index >= sampler->max_index) {
            sampler->index = 0;
        }
        sampler->samples++;
        sampler->counter = 0;
        if(sampler->factor_index <= sampler->target_factor) {
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

void queue_audio(Mixer* mixer) {
    uint32_t queue_size = SDL_GetAudioStreamQueued(mixer->audio_stream);
    mixer->stat = mixer->stat - mixer->stat_window[mixer->stat_index] + queue_size;
    mixer->stat_window[mixer->stat_index++] = queue_size;
    if(mixer->stat_index >= STATS_WIN_SIZE)
        mixer->stat_index = 0;

    size_t avg = mixer->stat / STATS_WIN_SIZE;
    // printf("queue size %d, avg: %llu \n", queue_size, avg);

    // From here we tweak the sampling rate ever so slightly to prevent underruns and runaway latency
    // by minimising deviation from the nominal queue size with a bit of control engineering
    float delta_f, error = (float)avg - NOMINAL_QUEUE_SIZE;
    Sampler* s = &mixer->sampler;
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

    SDL_PutAudioStreamData(mixer->audio_stream, mixer->buff, s->index * 2);
    // wait till queue is filled to prevent early onset underruns
    if(!mixer->audio_start && queue_size >= NOMINAL_QUEUE_SIZE) {
        pause_audio(mixer, 0);
        mixer->audio_start = 1;
    }
#if AUDIO_TO_FILE
    if(out_wav)
        fwrite(apu->buff, 2, s->index, out_wav);
#endif
    memset(mixer->buff, 0, AUDIO_BUFF_SIZE * 2);
    // reset sampler
    s->index = 0;
}

void pause_audio(Mixer* mixer, const int flag) {
    SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(mixer->audio_stream);
    int paused = SDL_AudioDevicePaused(dev);
    if(paused == flag)
        return;
    if(flag)
        SDL_PauseAudioDevice(dev);
    else
        SDL_ResumeAudioDevice(dev);
}

void free_mixer(const Mixer* mixer) {
    SDL_DestroyAudioStream(mixer->audio_stream);
}
#endif
