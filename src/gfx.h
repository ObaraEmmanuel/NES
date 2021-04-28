#pragma once

#include <SDL2/SDL.h>

typedef struct{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width;
    int height;
    float scale;

} GraphicsContext;

void free_graphics(GraphicsContext* ctx);

void get_graphics_context(GraphicsContext* ctx);

void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer);