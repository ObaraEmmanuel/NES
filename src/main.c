#include <stdlib.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <time.h>

#include "mapper.h"
#include "cpu6502.h"
#include "ppu.h"
#include "utils.h"
#include "gfx.h"
#include "controller.h"

#define BILLION 1000000000L
#define LATENCY_CHECK_COUNT 10

// frame rate in Hz, anything above 60 for some reason will not work
#define FRAME_RATE 60

// turbo keys toggle rate (Hz)
// value should be a factor of FRAME_RATE
// and should never exceed FRAME_RATE for best result
#define TURBO_RATE 30

static const uint64_t PERIOD = BILLION / FRAME_RATE;
static const uint16_t TURBO_SKIP = FRAME_RATE / TURBO_RATE;

static void init_sys(Mapper* mapper, PPU* ppu, c6502* cpu, Memory* mmu);
static int64_t timing_latency(struct timespec* start, struct timespec* end);
static int64_t compute_diff(struct timespec* start, struct timespec* end, int64_t latency);
static void turbo(Memory* mem);


int main(int argc, char *argv[]) {
    if(argc < 2) {
        LOG(ERROR, "Input file not provided");
        exit(EXIT_FAILURE);
    }
    Mapper mapper;
    load_file(argv[1], &mapper);
    c6502 cpu;
    PPU ppu;
    Memory mmu;

    srandom(time(0));

    init_sys(&mapper, &ppu, &cpu, &mmu);

    GraphicsContext g_ctx;
    g_ctx.width = 256;
    g_ctx.height = 240;
    g_ctx.scale = 2;
    get_graphics_context(&g_ctx);

    SDL_Event e;
    int8_t quit = 0, pause = 0;
    struct timespec m_start, m_end, start, end;
    int64_t latency = timing_latency(&start, &end);
    LOG(DEBUG, "Timing latency: %ld ns", latency);
    timespec_get(&m_start, TIME_UTC);
    while (!quit) {
        timespec_get(&start, TIME_UTC);
        while (SDL_PollEvent(&e)) {
            update_joypad(&mmu.joy1, &e);
            update_joypad(&mmu.joy2, &e);
            switch (e.type) {
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = 1;
                            break;
                        case SDLK_SPACE:
                            pause ^= 1;
                            break;
                        case SDLK_F5:
                            reset_cpu(&cpu);
                            LOG(INFO, "Resetting emulator");
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_QUIT:
                    quit = 1;
            }
        }
        // trigger turbo events
        turbo(&mmu);

        if(!pause){
            size_t c = 0;
            // 30000 cycles infinite loop fail safe in case render flag is not set
            // loop will on normal occasion hit scanline 261 long before that
            // if ppu.render is set a frame is complete
            while(!ppu.render && ++c < 30000){
                for (size_t i = 0; i < 3; i++)
                    execute_ppu(&ppu);
                execute(&cpu);
            }
            render_graphics(&g_ctx, ppu.screen);
            ppu.render = 0;
            timespec_get(&end, TIME_UTC);
            int64_t frame_time = compute_diff(&start, &end, latency);

            if(((int64_t)PERIOD - frame_time) >= 1000)
                usleep((PERIOD - frame_time) / 1000);
        }else{
            sleep(1);
        }
    }
     timespec_get(&m_end, TIME_UTC);
     unsigned long total_elapsed = m_end.tv_sec - m_start.tv_sec;
     LOG(DEBUG, "Average clock speed: %.2f MHz", ((double)cpu.t_cycles / 1000000) / (double)total_elapsed);
     LOG(DEBUG, "Average frame rate: %.2f Hz", (double)ppu.frames / (double)total_elapsed);


    free_graphics(&g_ctx);
    free_mapper(&mapper);
    return 0;
}


static void init_sys(Mapper* mapper, PPU* ppu, c6502* cpu, Memory* mmu){
    ppu->mapper = mmu->mapper = mapper;

    cpu->ppu = mmu->ppu = ppu;
    cpu->memory = ppu->mem = mmu;

    mmu->cpu = cpu;
    init_mem(mmu);
    init_ppu(ppu);
    init_cpu(cpu);
}

static void turbo(Memory* mem){
    if(mem->ppu->frames % TURBO_SKIP == 0) {
        turbo_trigger(&mem->joy1);
        turbo_trigger(&mem->joy2);
    }
}

static int64_t timing_latency(struct timespec* start, struct timespec* end){
    // compute the average timing latency
    int64_t total_latency = 0;
    for(size_t i = 0; i < LATENCY_CHECK_COUNT; i++) {
        timespec_get(start, TIME_UTC);
        timespec_get(end, TIME_UTC);
        total_latency += compute_diff(start, end, 0);
    }
    return total_latency / LATENCY_CHECK_COUNT;
}

static int64_t compute_diff(struct timespec* start, struct timespec* end, int64_t latency){
    return ((int64_t)end->tv_sec - (int64_t)start->tv_sec)*BILLION + ((int64_t)end->tv_nsec - (int64_t)start->tv_nsec) - latency;
}

