#include <SDL2/SDL.h>

#include "gfx.h"
#include "utils.h"


void get_graphics_context(GraphicsContext* ctx){

    SDL_Init(SDL_INIT_EVERYTHING);
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

    if(ctx->window == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_SetWindowMinimumSize(ctx->window, ctx->width, ctx->height);

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_PRESENTVSYNC);

    if(ctx->renderer == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_RenderSetLogicalSize(ctx->renderer, ctx->width, ctx->height);
    SDL_RenderSetIntegerScale(ctx->renderer, 1);

    ctx->texture = SDL_CreateTexture(
        ctx->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        ctx->width,
        ctx->height
    );

    if(ctx->texture == NULL){
        LOG(ERROR, SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_RenderSetScale(ctx->renderer, ctx->scale, ctx->scale);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_RenderPresent(ctx->renderer);

    LOG(DEBUG, "Initialized SDL subsystem");
}

void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer){
    SDL_RenderClear(g_ctx->renderer);
    SDL_UpdateTexture(g_ctx->texture, NULL, buffer, (int)(g_ctx->width * sizeof(uint32_t)));
    SDL_RenderCopy(g_ctx->renderer, g_ctx->texture, NULL, NULL);
    SDL_RenderPresent(g_ctx->renderer);
}

void free_graphics(GraphicsContext* ctx){
    SDL_DestroyWindow(ctx->window);
    SDL_DestroyRenderer(ctx->renderer);
    SDL_DestroyTexture(ctx->texture);
    SDL_Quit();
}
