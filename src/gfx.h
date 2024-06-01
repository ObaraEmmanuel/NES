#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#ifdef __ANDROID__
#include <SDL2/SDL_ttf.h>
#endif // __ANDROID__

typedef struct GraphicsContext{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audio_device;
    TTF_Font* font;
    SDL_Rect dest;
    int width;
    int height;
    int screen_width;
    int screen_height;
    float scale;

} GraphicsContext;

void free_graphics(GraphicsContext* ctx);

void get_graphics_context(GraphicsContext* ctx);

void render_graphics(GraphicsContext* g_ctx, const uint32_t* buffer);