#include <stdio.h>
#include <float.h>

#include "nsf_gfx.h"
#include "nsf.h"
#ifdef __ANDROID__
#include "touchpad.h"
#endif


#define BAR_COUNT 128

static float bins[BAR_COUNT] = {0};
static int amps[BAR_COUNT] = {0};
static double bin_boundaries[BAR_COUNT + 1] = {0};

static int song_num = -1;
static int minutes = -1, seconds = -1;

void init_NSF_graphics(Emulator* emulator, NSFGraphicsContext* nsf_ctx) {
    // re-init statics
    song_num = -1;
    minutes = -1;
    seconds = -1;

    nsf_ctx->emulator = emulator;

    const NSF* nsf = emulator->mapper.NSF;
    const GraphicsContext* g_ctx = &emulator->g_ctx;

#ifdef __ANDROID__
    int offset_x = g_ctx->dest.x, offset_y = g_ctx->dest.y, width = g_ctx->dest.w, height = g_ctx->dest.h;
#else
    int offset_x = 0, offset_y = 0;
#endif
    // pre-compute logarithmic binning boundaries
    for(size_t i = 0; i < BAR_COUNT; i++) {
        bin_boundaries[i] = exp((log(20000) - log(20))*i/(double)BAR_COUNT) * 20;
    }
    bin_boundaries[BAR_COUNT] = 20000;
    char buf[144] = {0};
    snprintf(buf, 144, "song: %s \nartist: %s \ncopyright: %s", nsf->song_name, nsf->artist, nsf->copyright);
    SDL_Color color = {192, 0x30, 0x0, 0xff};
    SDL_Surface* text_surf = TTF_RenderText_Blended_Wrapped(g_ctx->font, buf, 0, color, 0);
    nsf_ctx->song_info_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
    nsf_ctx->song_info_rect.w = text_surf->w;
    nsf_ctx->song_info_rect.h = text_surf->h;
    nsf_ctx->song_info_rect.x = 10 + offset_x;
    nsf_ctx->song_info_rect.y = 10 + offset_y;

    SDL_DestroySurface(text_surf);
}

void render_NSF_graphics(NSFGraphicsContext* nsf_ctx) {
    GraphicsContext* g_ctx = &nsf_ctx->emulator->g_ctx;
#ifdef __ANDROID__
    int offset_x = g_ctx->dest.x, offset_y = g_ctx->dest.y, width = g_ctx->dest.w, height = g_ctx->dest.h;
#else
    SDL_SetRenderScale(g_ctx->renderer, 1, 1);
    int offset_x = 0, offset_y = 0, width = g_ctx->width * g_ctx->scale, height = g_ctx->height * g_ctx->scale;
#endif

    APU* apu = &nsf_ctx->emulator->apu;
    complx* v = nsf_ctx->samples;
    const NSF* nsf = nsf_ctx->emulator->mapper.NSF;
    int silent = 1;
    // convert audio buffer to complex values for FFT
    for(size_t i =0; i < AUDIO_BUFF_SIZE; i++) {
        v[i].Re = apu->buff[i];
        v[i].Im = 0;
    }

    // FFT to extract frequency spectrum
    fft(nsf_ctx->samples, AUDIO_BUFF_SIZE, nsf_ctx->temp);

    // Place frequencies into their respective frequency bins
    double end = bin_boundaries[0], step = 20000.0f / AUDIO_BUFF_SIZE / 2, index = 0;
    size_t j = step/20;
    for(size_t i = 0; i < BAR_COUNT; i++) {
        double total = 0;
        size_t bin_count = 0;
        while(index < end) {
            total+=sqrt(v[j].Re * v[j].Re + v[j].Im * v[j].Im);
            index += step;
            j++;
            bin_count++;
        }
        end = bin_boundaries[i + 1];
        bins[i] = 0;
        if(!bin_count) {
            // distribute to empty bins for better visual effect
            int target = i > 0? i - 1 : 0;
            bins[i] = bins[target] / 2;
            if(target != i)
                bins[target] /= 2;
        } else {
            bins[i] = total / bin_count;
        }
    }

    // compute normalization factor for spectrum values for better visualization
    float min_v = FLT_MAX, max_v = FLT_MIN;
    for(size_t i = 0; i < BAR_COUNT; i++) {
        min_v = MIN(min_v, bins[i]);
        max_v = MAX(max_v, bins[i]);
    }
    float factor = 1.0f / (max_v - min_v);

    SDL_RenderClear(g_ctx->renderer);
    SDL_FRect dest;
    int max_bar_h = 0.4f * height, min_bar_h = 0.02f * height, bar_step = min_bar_h / 2;
    for(int i = 0; i < BAR_COUNT; i++) {
        int amp = factor * bins[i] * max_bar_h;
        // animate the visualization bars
        if(amp > amps[i]) {
            amps[i] += bar_step;
        }else {
            amps[i] -= bar_step;
        }
        amps[i] = amps[i] < min_bar_h ? min_bar_h : amps[i] > max_bar_h ? max_bar_h : amps[i];
        dest.y = (height - amps[i]) / 2 + offset_y;
        dest.x = i * width/BAR_COUNT + offset_x;
        dest.w = width/BAR_COUNT/2;
        dest.h = amps[i];
        SDL_SetRenderDrawColor(
            g_ctx->renderer, i*(g_ctx->width/BAR_COUNT), 0x0,
            g_ctx->width - (i * g_ctx->width/BAR_COUNT), 255);
        SDL_RenderFillRect(g_ctx->renderer, &dest);

    }

    int current_song = nsf->current_song == 0 ? 0 : nsf->current_song - 1;

    if(song_num != nsf->current_song) {
        SDL_DestroyTexture(nsf_ctx->song_num_tx);
        char str[32 + MAX_TRACK_NAME_SIZE] = {0};
        SDL_Color color = {0x62, 0x30, 152, 0xff};
        if(nsf->tlbls != NULL) {
            snprintf(
                str, 32 + MAX_TRACK_NAME_SIZE, "%d / %d: %s", nsf->current_song, nsf->total_songs,
                nsf->tlbls[current_song]
            );
        } else {
            snprintf(str, 32, "%d / %d", nsf->current_song, nsf->total_songs);
        }
        SDL_Surface* text_surf = TTF_RenderText_Blended(g_ctx->font, str, 0, color);
        nsf_ctx->song_num_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
        nsf_ctx->song_num_rect.h = text_surf->h;
        nsf_ctx->song_num_rect.w = text_surf->w;
        nsf_ctx->song_num_rect.x = 10 + offset_x;
        song_num = nsf->current_song;

        if(nsf->times != NULL) {
            SDL_DestroySurface(text_surf);
            SDL_DestroyTexture(nsf_ctx->song_dur_max_tx);
            snprintf(str, 8, "%02d : %02d", nsf->tick_max / 60000, ((long)nsf->tick_max % 60000) / 1000);
            SDL_Color color1 = {0x0, 0x30, 192, 0xff};
            text_surf = TTF_RenderText_Blended(g_ctx->font, str, 0, color1);
            nsf_ctx->song_dur_max_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
            nsf_ctx->song_dur_max_rect.h = text_surf->h;
            nsf_ctx->song_dur_max_rect.w = text_surf->w;
            nsf_ctx->song_dur_max_rect.x = (width - text_surf->w - 10) + offset_x;
            nsf_ctx->song_dur_max_rect.y = height - 15 - text_surf->h + offset_y;
            nsf_ctx->song_num_rect.y = nsf_ctx->song_dur_max_rect.y - 0.12*height - text_surf->h;
        }else {
            nsf_ctx->song_num_rect.x = (width - text_surf->w) / 2 + offset_x;;
            nsf_ctx->song_num_rect.y = height - 15 - text_surf->h + offset_y;
        }
        SDL_DestroySurface(text_surf);
    }

    if(nsf->times != NULL) {
        int cur_min = nsf->tick / 60000;
        int cur_sec = ((long)nsf->tick % 60000) / 1000;

        dest.x = offset_x + 10;
        dest.y = nsf_ctx->song_dur_max_rect.y - 0.06*height;
        dest.w = width - 20;
        dest.h = 0.01 * height;
        SDL_SetRenderDrawColor(g_ctx->renderer, 30, 30, 30, 0x1f);
        SDL_RenderFillRect(g_ctx->renderer, &dest);

        dest.w = (width - 20) * nsf->tick / nsf->tick_max;
        SDL_SetRenderDrawColor(g_ctx->renderer, 60, 0x30, 192, 0xff);
        SDL_RenderFillRect(g_ctx->renderer, &dest);

        if(cur_min != minutes || cur_sec != seconds) {
            SDL_DestroyTexture(nsf_ctx->song_dur_tx);
            char str[8];
            SDL_Color color = {0x0, 0x30, 192, 0xff};
            snprintf(str, 8, "%02d : %02d", cur_min, cur_sec);
            SDL_Surface* text_surf = TTF_RenderText_Blended(g_ctx->font, str, 0, color);
            nsf_ctx->song_dur_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
            nsf_ctx->song_dur_rect.h = text_surf->h;
            nsf_ctx->song_dur_rect.w = text_surf->w;
            nsf_ctx->song_dur_rect.x = offset_x + 10;
            nsf_ctx->song_dur_rect.y = height - 15 - text_surf->h + offset_y;
            SDL_DestroySurface(text_surf);
            minutes = cur_min;
            seconds = cur_sec;
        }
    }

    SDL_RenderTexture(g_ctx->renderer, nsf_ctx->song_info_tx, NULL, &nsf_ctx->song_info_rect);
    SDL_RenderTexture(g_ctx->renderer, nsf_ctx->song_num_tx, NULL, &nsf_ctx->song_num_rect);
    SDL_RenderTexture(g_ctx->renderer, nsf_ctx->song_dur_tx, NULL, &nsf_ctx->song_dur_rect);
    SDL_RenderTexture(g_ctx->renderer, nsf_ctx->song_dur_max_tx, NULL, &nsf_ctx->song_dur_max_rect);
#ifdef __ANDROID__
    ANDROID_RENDER_TOUCH_CONTROLS(g_ctx);
#endif
    SDL_SetRenderDrawColor(g_ctx->renderer, 0, 0, 0, 255);
    SDL_RenderPresent(g_ctx->renderer);
}

void free_NSF_graphics(NSFGraphicsContext* nsf_ctx) {
    SDL_DestroyTexture(nsf_ctx->song_num_tx);
    SDL_DestroyTexture(nsf_ctx->song_info_tx);
    SDL_DestroyTexture(nsf_ctx->song_dur_tx);
    SDL_DestroyTexture(nsf_ctx->song_dur_max_tx);
}
