#include "nsf.h"
#include "utils.h"
#include "emulator.h"
#include "font.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#define PRG_ROM_SIZE 0x8000
#define PRG_RAM_SIZE 0x2000
#define BAR_COUNT 128

static uint8_t read_PRG(Mapper*, uint16_t);
static void write_PRG(Mapper*, uint16_t, uint8_t);
static uint8_t read_CHR(Mapper*, uint16_t);
static void write_CHR(Mapper*, uint16_t, uint8_t);
static uint8_t read_ROM(Mapper*, uint16_t);
static void write_ROM(Mapper*, uint16_t, uint8_t);

void load_nsf(SDL_RWops* file, Mapper* mapper) {
    SDL_RWseek(file, 0, RW_SEEK_SET);

    uint8_t header[NSF_HEADER_SIZE];
    SDL_RWread(file, header, NSF_HEADER_SIZE, 1);

    mapper->is_nsf = 1;

    NSF* nsf = calloc(1, sizeof(NSF));
    mapper->NSF = nsf;

    nsf->version = header[5];
    nsf->total_songs = header[6];
    nsf->starting_song = nsf->current_song = header[7];

    nsf->load_addr = (header[9] << 8) | header[8];
    if(nsf->load_addr < 0x8000) {
        LOG(INFO, "Load address ox%04x too low", nsf->load_addr);
        exit(EXIT_FAILURE);
    }

    nsf->init_addr = (header[0xb] << 8) | header[0xa];
    nsf->play_addr = (header[0xd] << 8) | header[0xc];

    strncpy(nsf->song_name, (char*)(header + 0xe), TEXT_FIELD_SIZE);
    strncpy(nsf->artist, (char*)(header + 0x2e), TEXT_FIELD_SIZE);
    strncpy(nsf->copyright, (char*)(header + 0x4e), TEXT_FIELD_SIZE);

    LOG(INFO, "SONG_NAME: %s", nsf->song_name);
    LOG(INFO, "ARTIST: %s", nsf->artist);
    LOG(INFO, "COPYRIGHT: %s", nsf->copyright);

    if(header[0x7a] & BIT_1) {
        // PAL/NTSC
        LOG(INFO, "ROM type: Dual Compatible (using NTSC)");
        mapper->type = NTSC;
    }else if(header[0x7a] & BIT_0) {
        // PAL
        LOG(INFO, "ROM type: PAL");
        mapper->type = PAL;
    }else {
        // NTSC
        LOG(INFO, "ROM type: NTSC");
        mapper->type = NTSC;
    }

    if(mapper->type == PAL) {
        nsf->speed = (header[0x79] << 8) | header[0x78];
    }else {
        nsf->speed = (header[0x6f] << 8) | header[0x6e];
    }

    if(header[0x7b]) {
        LOG(ERROR, "Extra Sound Chip support required");
        exit(EXIT_FAILURE);
    }

    size_t data_len = (header[0x7f] << 16) | (header[0x7e] << 8) | header[0x7d];
    if(!data_len || nsf->version == 1) {
        long long size = SDL_RWseek(file, 0, RW_SEEK_END);
        if(size < 0) {
            LOG(ERROR, "Error reading ROM");
            exit(EXIT_FAILURE);
        }
        data_len = size - NSF_HEADER_SIZE;
        // reset file ptr
        SDL_RWseek(file, NSF_HEADER_SIZE, RW_SEEK_SET);
    }
    LOG(INFO, "Program data length: %llu", data_len);

    nsf->bank_switch = 0;
    for(size_t i = 0x70; i < 0x78; i++) {
        if(header[i] != 0) {
            nsf->bank_switch = 1;
            break;
        }
    }
    LOG(INFO, "Bank switching: %s", nsf->bank_switch ? "ON": "OFF");

    // PRG RAM
    mapper->save_RAM = malloc(PRG_RAM_SIZE);
    memset(mapper->save_RAM, 0, PRG_RAM_SIZE);

    // mapper R/W redirects
    mapper->read_PRG = read_PRG;
    mapper->write_PRG = write_PRG;
    mapper->read_CHR = read_CHR;
    mapper->write_CHR = write_CHR;
    mapper->read_ROM = read_ROM;
    mapper->write_ROM = write_ROM;


    if(nsf->bank_switch) {
        uint16_t padding = nsf->load_addr & 0xfff;
        size_t prg_size = data_len + padding;
        if(prg_size % 0x1000) {
            // ensure size is a multiple of 4kb
            prg_size = (prg_size / 0x1000 + 1) * 0x1000;
            mapper->PRG_banks = prg_size / 0X1000;
        }
        LOG(INFO, "PRG banks: %llu", mapper->PRG_banks);
        mapper->PRG_ROM = malloc(prg_size);
        memset(mapper->PRG_ROM, 0, prg_size);
        SDL_RWread(file, mapper->PRG_ROM + padding, 1, data_len);

        // init banks
        for(size_t i =0; i < 8; i++) {
            mapper->write_ROM(mapper, 0x5ff8 + i, header[0x70+i]);
            nsf->bank_init[i] = header[0x70+i];
        }

    } else {
        mapper->PRG_ROM = malloc(PRG_ROM_SIZE);
        memset(mapper->PRG_ROM, 0, PRG_ROM_SIZE);
        size_t read_len = MIN(data_len, 0x10000 - nsf->load_addr);
        SDL_RWread(file, mapper->PRG_ROM + (nsf->load_addr - 0x8000), 1, read_len);
    }
}

static uint8_t read_ROM(Mapper* mapper, uint16_t addr) {
    if(addr < 0x6000)
        return 0;

    if(addr < 0x8000)
        return mapper->save_RAM[addr - 0x6000];

    return mapper->read_PRG(mapper, addr);
}

static void write_ROM(Mapper* mapper, uint16_t addr, uint8_t val) {
    if(addr < 0x6000) {
        if(addr > 0x5ff7) {
            mapper->NSF->bank_ptrs[addr - 0x5ff8] = mapper->PRG_ROM + val * 0x1000;
        }
    } else if(addr < 0x8000) {
        mapper->save_RAM[addr - 0x6000] = val;
    }
}

static uint8_t read_PRG(Mapper* mapper, uint16_t addr) {
    if(!mapper->NSF->bank_switch) {
        return mapper->PRG_ROM[addr - 0x8000];
    }
    size_t bank_index = (addr - 0x8000) / 0x1000;
    return *(mapper->NSF->bank_ptrs[bank_index] + (addr & 0xfff));
}

static void write_PRG(Mapper* mapper, uint16_t addr, uint8_t val) {
    // can't write to PRG ROM
}

static uint8_t read_CHR(Mapper* mapper, uint16_t addr) {
    // CHR not required
    return 0;
}

static void write_CHR(Mapper* mapper, uint16_t addr, uint8_t val) {
    // CHR not required
}

void init_song(Emulator* emulator, size_t song_number) {
    memset(emulator->mem.RAM, 0, RAM_SIZE);
    init_cpu(emulator);
    emulator->apu.audio_start = 0;
    emulator->apu.sampler.index = 0;
    SDL_PauseAudioDevice(emulator->g_ctx.audio_device, 1);

    for(size_t i = 0; i < 14; i++) {
        write_mem(&emulator->mem, 0x4000 + i, 0);
    }
    write_mem(&emulator->mem, 0x4015, 0);
    write_mem(&emulator->mem, 0x4015, 0xf);
    write_mem(&emulator->mem, 0x4017, 0x40);

    NSF* nsf = emulator->mapper.NSF;
    if(nsf->bank_switch) {
        for(size_t i = 0; i < 8; i++) {
            write_mem(&emulator->mem, 0x5ff8 + i, nsf->bank_init[i]);
        }
    }
    emulator->cpu.ac = song_number > 0 ? song_number - 1: song_number;
    emulator->cpu.x = emulator->type == PAL? 1: 0;
    nsf->initializing = 1;
    nsf_jsr(emulator, nsf->init_addr);
    LOG(DEBUG, "Initializing tune %d", nsf->current_song);
}

void nsf_jsr(Emulator* emulator, uint16_t address) {
    // approximate JSR
    uint16_t ret_addr = NSF_SENTINEL_ADDR - 1;
    write_mem(&emulator->mem, STACK_START + emulator->cpu.sp--, ret_addr >> 8);
    write_mem(&emulator->mem, STACK_START + emulator->cpu.sp--, ret_addr & 0xFF);
    emulator->cpu.pc = address;
}

static double bin_boundaries[BAR_COUNT + 1] = {0};

void init_NSF_gfx(GraphicsContext* g_ctx, NSF* nsf) {
    // pre-compute logarithmic binning boundaries
    for(size_t i = 0; i < BAR_COUNT; i++) {
        bin_boundaries[i] = exp((log(24000) - log(20))*i/(double)BAR_COUNT) * 20;
    }
    bin_boundaries[BAR_COUNT] = 24000;

    SDL_RWops* rw = SDL_RWFromMem(font_data, sizeof(font_data));
    g_ctx->font = TTF_OpenFontRW(rw, 1, 11);
    if(g_ctx->font == NULL){
        LOG(ERROR, SDL_GetError());
    }
    char buf[144] = {0};
    snprintf(buf, 144, "song: %s \nartist: %s \ncopyright: %s", nsf->song_name, nsf->artist, nsf->copyright);
    SDL_Color color = {192, 0x30, 0x0, 0xff};
    SDL_Surface* text_surf = TTF_RenderUTF8_Solid_Wrapped(g_ctx->font, buf, color, 0);
    nsf->song_info_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
    nsf->song_info_rect.w = text_surf->w;
    nsf->song_info_rect.h = text_surf->h;
    nsf->song_info_rect.x = nsf->song_info_rect.y = 10;

    SDL_FreeSurface(text_surf);
}

static float bins[BAR_COUNT] = {0};
static int amps[BAR_COUNT] = {0};

void render_NSF_graphics(Emulator* emulator, NSF* nsf) {
    static int song_num = -1;

    GraphicsContext* g_ctx = &emulator->g_ctx;
    APU* apu = &emulator->apu;
    complx* v = nsf->samples;
    // convert audio buffer to complex values for FFT
    for(size_t i =0; i < AUDIO_BUFF_SIZE; i++) {
        v[i].Re = apu->buff[i];
        v[i].Im = 0;
    }
    // FFT to extract frequency spectrum
    fft(nsf->samples, AUDIO_BUFF_SIZE, nsf->temp);

    // Place frequencies into their respective frequency bins
    double end = bin_boundaries[0], step = 24000.0f / AUDIO_BUFF_SIZE / 2, index = 0;
    size_t j = 0;
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
        min_v = min(min_v, bins[i]);
        max_v = max(max_v, bins[i]);
    }
    float factor = 1.0f / (max_v - min_v);

    SDL_RenderClear(g_ctx->renderer);
    SDL_Rect dest;
    int amp;
    for(int i = 0; i < BAR_COUNT; i++) {
        amp = factor * bins[i] * 100;
        // animate the visualization bars
        if(amp > amps[i]) {
            amps[i] += 2;
        }else {
            amps[i] -= 2;
        }
        amps[i] = amps[i] < 4 ? 4 : amps[i] > 100 ? 100 : amps[i];
        dest.y = (g_ctx->height - amps[i]) / 2;
        dest.x = i * g_ctx->width/BAR_COUNT;
        dest.w = g_ctx->width/BAR_COUNT - 1;
        dest.h = amps[i];
        SDL_SetRenderDrawColor(
            g_ctx->renderer, i*(g_ctx->width/BAR_COUNT), 0x0,
            g_ctx->width - (i * g_ctx->width/BAR_COUNT), 255);
        SDL_RenderFillRect(g_ctx->renderer, &dest);

    }

    if(song_num != nsf->current_song) {
        SDL_DestroyTexture(nsf->song_num_tx);
        char str[32];
        SDL_Color color = {0x0, 0x30, 192, 0xff};
        snprintf(str, 32, "[%d / %d]", nsf->current_song, nsf->total_songs);
        SDL_Surface* text_surf = TTF_RenderText_Solid(g_ctx->font, str, color);
        nsf->song_num_tx = SDL_CreateTextureFromSurface(g_ctx->renderer, text_surf);
        nsf->song_num_rect.h = text_surf->h;
        nsf->song_num_rect.w = text_surf->w;
        nsf->song_num_rect.x = (g_ctx->width - text_surf->w) / 2;
        nsf->song_num_rect.y = g_ctx->height - 10 - text_surf->h;
        SDL_FreeSurface(text_surf);
        song_num = nsf->current_song;
    }

    SDL_RenderCopy(g_ctx->renderer, nsf->song_info_tx, NULL, &nsf->song_info_rect);
    SDL_RenderCopy(g_ctx->renderer, nsf->song_num_tx, NULL, &nsf->song_num_rect);
    SDL_SetRenderDrawColor(g_ctx->renderer, 0, 0, 0, 255);
    SDL_RenderPresent(g_ctx->renderer);
}
