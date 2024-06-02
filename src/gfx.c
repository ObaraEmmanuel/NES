#include <SDL.h>

#include "gfx.h"
#include "utils.h"
#include "font.h"

#ifdef __ANDROID__
#include "touchpad.h"
#endif

void get_graphics_context(GraphicsContext* ctx){

    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
#ifdef __ANDROID__
    ctx->font = TTF_OpenFont("asap.ttf", (int)(ctx->screen_height * 0.05));
    if(ctx->font == NULL){
        LOG(ERROR, SDL_GetError());
    }
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    ctx->window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        // width and height not used in FULLSCREEN
        0,
        0,
        SDL_WINDOW_FULLSCREEN_DESKTOP
        | SDL_WINDOW_ALLOW_HIGHDPI
    );
#else
    SDL_RWops* rw = SDL_RWFromMem(font_data, sizeof(font_data));
    ctx->font = TTF_OpenFontRW(rw, 1, 11);
    if(ctx->font == NULL){
        LOG(ERROR, SDL_GetError());
    }
    ctx->window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ctx->width * (int)ctx->scale,
        ctx->height * (int)ctx->scale,
        SDL_WINDOW_SHOWN
        | SDL_WINDOW_OPENGL
        | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_ALLOW_HIGHDPI
    );
#endif

    if(ctx->window == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_SetWindowMinimumSize(ctx->window, ctx->width, ctx->height);

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED);
    if(ctx->renderer == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }

#ifdef __ANDROID__
    ctx->dest.h = ctx->screen_height;
    ctx->dest.w = (ctx->width * ctx->dest.h) / ctx->height;
    ctx->dest.x = (ctx->screen_width - ctx->dest.w) / 2;
    ctx->dest.y = 0;
#else
    SDL_RenderSetLogicalSize(ctx->renderer, ctx->width, ctx->height);
    SDL_RenderSetIntegerScale(ctx->renderer, 1);
    SDL_RenderSetScale(ctx->renderer, ctx->scale, ctx->scale);
#endif

    ctx->texture = SDL_CreateTexture(
        ctx->renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_TARGET,
        ctx->width,
        ctx->height
    );

    if(ctx->texture == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderPresent(ctx->renderer);

    LOG(DEBUG, "Initialized SDL subsystem");
}

void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer){
    SDL_RenderClear(g_ctx->renderer);
    SDL_UpdateTexture(g_ctx->texture, NULL, buffer, (int)(g_ctx->width * sizeof(uint32_t)));
#ifdef __ANDROID__
    SDL_RenderCopy(g_ctx->renderer, g_ctx->texture, NULL, &g_ctx->dest);
    ANDROID_RENDER_TOUCH_CONTROLS(g_ctx);
#else
    SDL_RenderCopy(g_ctx->renderer, g_ctx->texture, NULL, NULL);
#endif
    SDL_SetRenderDrawColor(g_ctx->renderer, 0, 0, 0, 255);
    SDL_RenderPresent(g_ctx->renderer);
}

void free_graphics(GraphicsContext* ctx){
    TTF_CloseFont(ctx->font);
    TTF_Quit();
    SDL_DestroyTexture(ctx->texture);
    SDL_DestroyRenderer(ctx->renderer);
    SDL_DestroyWindow(ctx->window);
    SDL_CloseAudioDevice(ctx->audio_device);
    SDL_Quit();
    LOG(DEBUG, "Graphics clean up complete");
}
