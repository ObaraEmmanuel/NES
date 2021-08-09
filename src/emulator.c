#include <SDL2/SDL.h>

#include "emulator.h"
#include "gamepad.h"
#include "touchpad.h"
#include "controller.h"
#include "gfx.h"
#include "mapper.h"


void init_emulator(struct Emulator* emulator, int argc, char *argv[]){
    if(argc < 2) {
        LOG(ERROR, "Input file not provided");
        exit(EXIT_FAILURE);
    }

    char* genie = NULL;
    if(argc == 3 || argc == 5)
        genie = argv[argc - 1];

    load_file(argv[1], genie, &emulator->mapper);
    init_mem(emulator);
    init_ppu(emulator);
    init_cpu(emulator);

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
    get_graphics_context(g_ctx);

    ANDROID_INIT_TOUCH_PAD(g_ctx);
    init_pads();

    emulator->exit = 0;
    emulator->pause = 0;

}


void run_emulator(struct Emulator* emulator){
    struct JoyPad* joy1 = &emulator->mem.joy1;
    struct JoyPad* joy2 = &emulator->mem.joy2;
    struct PPU* ppu = &emulator->ppu;
    struct c6502* cpu = &emulator->cpu;
    struct GraphicsContext* g_ctx = &emulator->g_ctx;
    SDL_Event e;
    uint32_t start, end;
    emulator->m_start = SDL_GetTicks();

    while (!emulator->exit) {
#if PROFILE
        if(ppu->frames >= PROFILE_STOP_FRAME)
            break;
#endif
        start = SDL_GetTicks();
        while (SDL_PollEvent(&e)) {
            update_joypad(joy1, &e);
            update_joypad(joy2, &e);
            switch (e.type) {
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            emulator->exit = 1;
                            break;
                        case SDLK_SPACE:
                            emulator->pause ^= 1;
                            break;
                        case SDLK_F5:
                            reset_cpu(cpu);
                            LOG(INFO, "Resetting emulator");
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_QUIT:
                    emulator->exit = 1;
                    break;
                default:
                    if((e.key.keysym.sym == SDLK_AC_BACK
                        || e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) && !emulator->exit) {
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
            size_t c = 0;
            // 30000 cycles infinite loop fail-safe in case render flag is not set
            // loop will on normal occasion hit scanline 261 long before that
            // if ppu.render is set a frame is complete
            while(!ppu->render){
                execute_ppu(ppu);
                execute_ppu(ppu);
                execute_ppu(ppu);
                execute(cpu);
            }
            render_graphics(g_ctx, ppu->screen);
            ppu->render = 0;
            end = SDL_GetTicks();
            uint32_t diff = end - start;
            if(diff < PERIOD)
                SDL_Delay(PERIOD - diff);
        }else{
            SDL_Delay(IDLE_SLEEP);
        }
    }

    emulator->m_end = SDL_GetTicks();
}

void free_emulator(struct Emulator* emulator){
    LOG(DEBUG, "Starting emulator clean up");
    free_mapper(&emulator->mapper);
    ANDROID_FREE_TOUCH_PAD();
    free_graphics(&emulator->g_ctx);
    LOG(DEBUG, "Emulator session successfully terminated");
}