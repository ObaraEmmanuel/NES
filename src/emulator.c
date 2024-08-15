#include <SDL.h>

#include "emulator.h"
#include "gamepad.h"
#include "touchpad.h"
#include "controller.h"
#include "gfx.h"
#include "mapper.h"
#include "nsf.h"
#include "timers.h"
#include "debugtools.h"
#include "utils.h"

static uint64_t PERIOD;
static uint16_t TURBO_SKIP;

void init_emulator(struct Emulator* emulator, int argc, char *argv[]){
    if(argc < 2) {
        LOG(ERROR, "Input file not provided");
        quit(EXIT_FAILURE);
    }

    char* genie = NULL;
    if(argc == 3 || argc == 5)
        genie = argv[argc - 1];

    load_file(argv[1], genie, &emulator->mapper);
    emulator->type = emulator->mapper.type;
    emulator->mapper.emulator = emulator;
    if(emulator->type == PAL) {
        PERIOD = 1000000000 / PAL_FRAME_RATE;
        TURBO_SKIP = PAL_FRAME_RATE / PAL_TURBO_RATE;
    }else{
        PERIOD = 1000000000 / NTSC_FRAME_RATE;
        TURBO_SKIP = NTSC_FRAME_RATE / NTSC_TURBO_RATE;
    }

    GraphicsContext* g_ctx = &emulator->g_ctx;

#ifdef __ANDROID__
    if(argc < 4){
        LOG(ERROR, "Window dimensions not provided");
        return;
    }
    char** end_ptr = NULL;
    g_ctx->screen_width = strtol(argv[2], end_ptr, 10);
    g_ctx->screen_height = strtol(argv[3], end_ptr, 10);
#else
    g_ctx->screen_width = -1;
    g_ctx->screen_height = -1;
#endif

    g_ctx->width = 256;
    g_ctx->height = 240;
    g_ctx->scale = 2;

#if NAMETABLE_MODE
    g_ctx->width = 512;
    g_ctx->height = 480;
    g_ctx->scale = 1;
    if(emulator->mapper.is_nsf) {
        LOG(ERROR, "Can't run NSF Player in Nametable mode");
        quit(EXIT_FAILURE);
    }
    LOG(DEBUG, "RENDERING IN NAMETABLE MODE");
#endif
    get_graphics_context(g_ctx);
    SDL_SetWindowTitle(g_ctx->window, get_file_name(argv[1]));

    init_mem(emulator);
    init_ppu(emulator);
    init_cpu(emulator);
    init_APU(emulator);
    init_timer(&emulator->timer, PERIOD);
    ANDROID_INIT_TOUCH_PAD(g_ctx);
    init_pads();

    emulator->exit = 0;
    emulator->pause = 0;
}


void run_emulator(struct Emulator* emulator){
    if(emulator->mapper.is_nsf) {
        run_NSF_player(emulator);
        return;
    }

    struct JoyPad* joy1 = &emulator->mem.joy1;
    struct JoyPad* joy2 = &emulator->mem.joy2;
    struct PPU* ppu = &emulator->ppu;
    struct c6502* cpu = &emulator->cpu;
    struct APU* apu = &emulator->apu;
    struct GraphicsContext* g_ctx = &emulator->g_ctx;
    struct Timer* timer = &emulator->timer;
    SDL_Event e;
    Timer frame_timer;
    init_timer(&frame_timer, PERIOD);
    mark_start(&frame_timer);

    while (!emulator->exit) {
#if PROFILE
        if(PROFILE_STOP_FRAME && ppu->frames >= PROFILE_STOP_FRAME)
            break;
#endif
        mark_start(timer);
        while (SDL_PollEvent(&e)) {
            update_joypad(joy1, &e);
            update_joypad(joy2, &e);
            if((joy1->status & 0xc) == 0xc || (joy2->status & 0xc) == 0xc) {
                reset_emulator(emulator);
            }
            switch (e.type) {
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            emulator->exit = 1;
                            break;
                        case SDLK_AUDIOPLAY:
                        case SDLK_SPACE:
                            emulator->pause ^= 1;
                            TOGGLE_TIMER_RESOLUTION();
                            break;
                        case SDLK_F5:
                            reset_emulator(emulator);
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_QUIT:
                    emulator->exit = 1;
                    break;
                default:
                    if(e.key.keysym.sym == SDLK_AC_BACK
                        || e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
                        emulator->exit = 1;
                        LOG(DEBUG, "Exiting emulator session");
                    }
            }
        }

        // trigger turbo events
        if(ppu->frames % TURBO_SKIP == 0) {
            turbo_trigger(joy1);
            turbo_trigger(joy2);
        }

        if(!emulator->pause){
            // if ppu.render is set a frame is complete
            if(emulator->type == NTSC) {
                while (!ppu->render) {
                    execute_ppu(ppu);
                    execute_ppu(ppu);
                    execute_ppu(ppu);
                    execute(cpu);
                    execute_apu(apu);
                }
            }else{
                // PAL
                uint8_t check = 0;
                while (!ppu->render) {
                    execute_ppu(ppu);
                    execute_ppu(ppu);
                    execute_ppu(ppu);
                    check++;
                    if(check == 5) {
                        // on the fifth run execute an extra ppu clock
                        // this produces 3.2 scanlines per cpu clock
                        execute_ppu(ppu);
                        check = 0;
                    }
                    execute(cpu);
                    execute_apu(apu);
                }
            }
#if NAMETABLE_MODE
            render_name_tables(ppu, ppu->screen);
#endif
            render_graphics(g_ctx, ppu->screen);
            ppu->render = 0;
            queue_audio(apu, g_ctx);
            mark_end(timer);
            adjusted_wait(timer);
        }else{
            wait(IDLE_SLEEP);
        }
    }

    mark_end(&frame_timer);
    emulator->time_diff = get_diff_ms(&frame_timer);
    release_timer(&frame_timer);
}

void reset_emulator(Emulator* emulator) {
    LOG(INFO, "Resetting emulator");
    reset_cpu(&emulator->cpu);
    reset_APU(&emulator->apu);
    reset_ppu(&emulator->ppu);
    if(emulator->mapper.reset != NULL) {
        emulator->mapper.reset(&emulator->mapper);
    }
}

void run_NSF_player(struct Emulator* emulator) {
    LOG(INFO, "Starting NSF player...");
    JoyPad* joy1 = &emulator->mem.joy1;
    JoyPad* joy2 = &emulator->mem.joy2;
    c6502* cpu = &emulator->cpu;
    APU* apu = &emulator->apu;
    NSF* nsf = emulator->mapper.NSF;
    GraphicsContext* g_ctx = &emulator->g_ctx;
    init_NSF_gfx(g_ctx, nsf);
    Timer* timer = &emulator->timer;
    SDL_Event e;
    Timer frame_timer;
    PERIOD = 1000 * emulator->mapper.NSF->speed;
    double ms_per_frame = emulator->mapper.NSF->speed / 1000.0;
    init_timer(&frame_timer, PERIOD);
    mark_start(&frame_timer);
    size_t cycles_per_frame;
    if(emulator->type == PAL) {
        cycles_per_frame = emulator->mapper.NSF->speed * 1.662607f;
    }else {
        cycles_per_frame = emulator->mapper.NSF->speed * 1.789773f;
    }
    uint8_t status1 = 0, status2 = 0;

    // initialize for the first song
    init_song(emulator, nsf->current_song);


    while (!emulator->exit) {
        mark_start(timer);
        while (SDL_PollEvent(&e)) {
            update_joypad(joy1, &e);
            update_joypad(joy2, &e);
            if((status1 & RIGHT && !(joy1->status & RIGHT)) || (status2 & RIGHT && !(joy2->status & RIGHT))) {
                next_song(emulator, nsf);
            } else if((status1 & LEFT && !(joy1->status & LEFT)) || (status2 & LEFT && !(joy2->status & LEFT))) {
                prev_song(emulator, nsf);
            } else if((status1 & START && !(joy1->status & START)) || (status2 & START && !(joy2->status & START))) {
                emulator->pause ^= 1;
            }
            status1 = joy1->status;
            status2 = joy2->status;
            if((joy1->status & 0xc) == 0xc || (joy2->status & 0xc) == 0xc) {
                reset_emulator(emulator);
                nsf->current_song = 1;
                init_song(emulator, nsf->current_song);
            }

            switch (e.type) {
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            emulator->exit = 1;
                            break;
                        case SDLK_AUDIOPLAY:
                        case SDLK_SPACE:
                            emulator->pause ^= 1;
                            TOGGLE_TIMER_RESOLUTION();
                            break;
                        case SDLK_AUDIONEXT:
                            next_song(emulator, nsf);
                            break;
                        case SDLK_AUDIOPREV:
                            prev_song(emulator, nsf);
                            break;
                        case SDLK_F5:
                            reset_emulator(emulator);
                            nsf->current_song = 1;
                            init_song(emulator, nsf->current_song);
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_QUIT:
                    emulator->exit = 1;
                    break;
                default:
                    if(e.key.keysym.sym == SDLK_AC_BACK
                        || e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
                        emulator->exit = 1;
                        LOG(DEBUG, "Exiting emulator session");
                    }
            }
        }

        if(nsf->times != NULL && !nsf->initializing) {
            double track_dur = nsf->times[nsf->current_song == 0 ? 0 : nsf->current_song - 1];
            if(track_dur < nsf->tick) {
                if(nsf->tick_max < nsf->tick) {
                    next_song(emulator, nsf);
                } else if(nsf->fade != NULL) {
                    // fade
                    int fade_dur = nsf->fade[nsf->current_song == 0 ? 0 : nsf->current_song - 1];
                    apu->volume = (nsf->tick - track_dur) / (float)fade_dur;
                    // clamp to range (0,1) then invert
                    apu->volume = 1 - (apu->volume < 0 ? 0 : apu->volume > 1 ? 1 : apu->volume);
                }
            }
        }

        if(cpu->pc == NSF_SENTINEL_ADDR) {
            nsf_jsr(emulator, nsf->play_addr);
        }

        if(!emulator->pause){
            size_t cycles = 0;
            while (cycles < cycles_per_frame) {
                // run CPU if RTS has not been called
                if(cpu->pc != NSF_SENTINEL_ADDR)
                    execute(cpu);
                if(!nsf->initializing)
                    execute_apu(apu);
                cycles++;
            }

            render_NSF_graphics(emulator, nsf);
            if(!nsf->initializing) {
                queue_audio(apu, g_ctx);
                nsf->tick += ms_per_frame;
            }
            if(cpu->pc == NSF_SENTINEL_ADDR && nsf->initializing) {
                nsf->initializing = 0;
                nsf->tick = 0;
            }
            mark_end(timer);
            adjusted_wait(timer);
        }else{
            wait(IDLE_SLEEP);
        }
    }

    mark_end(&frame_timer);
    emulator->time_diff = get_diff_ms(&frame_timer);
    release_timer(&frame_timer);
}


void free_emulator(struct Emulator* emulator){
    LOG(DEBUG, "Starting emulator clean up");
    exit_APU();
    exit_ppu(&emulator->ppu);
    free_mapper(&emulator->mapper);
    ANDROID_FREE_TOUCH_PAD();
    free_graphics(&emulator->g_ctx);
    release_timer(&emulator->timer);
    LOG(DEBUG, "Emulator session successfully terminated");
}