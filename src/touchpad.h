#pragma once

#ifdef __ANDROID__

#include <stdint.h>
#include <SDL.h>

#include "gfx.h"
#include "controller.h"

typedef struct TouchAxis{
    int x;
    int y;
    int r;
    int inner_x;
    int inner_y;
    int origin_x;
    int origin_y;
    uint8_t state;
    uint8_t active;
    uint8_t h_latch;
    uint8_t v_latch;
    SDL_FingerID finger;
    SDL_Rect bg_dest;
    SDL_Rect joy_dest;
    SDL_Texture* bg_tx;
    SDL_Texture* joy_tx;
} TouchAxis;


typedef struct  TouchButton{
    SDL_Texture * texture;
    SDL_Rect dest;
    KeyPad id;
    int x;
    int y;
    int r;
    uint8_t active;
    uint8_t auto_render;
    SDL_FingerID finger;
} TouchButton;

#define TOUCH_BUTTON_COUNT 6

typedef struct TouchPad{
    uint16_t status;
    TouchButton A;
    TouchButton turboA;
    TouchButton B;
    TouchButton turboB;
    TouchButton select;
    TouchButton start;
    TouchButton* buttons[TOUCH_BUTTON_COUNT];
    TouchAxis axis;
    GraphicsContext* g_ctx;
    TTF_Font * font;
} TouchPad;

// forward declaration
struct JoyPad;

void init_touch_pad(GraphicsContext* ctx);
void free_touch_pad();
void render_touch_controls(GraphicsContext* ctx);
void touchpad_mapper(struct JoyPad* joyPad, SDL_Event* event);

#define ANDROID_INIT_TOUCH_PAD(CTX) init_touch_pad(CTX)
#define ANDROID_FREE_TOUCH_PAD() free_touch_pad()
#define ANDROID_RENDER_TOUCH_CONTROLS(CTX) render_touch_controls(CTX)
#define ANDROID_TOUCHPAD_MAPPER(JOYPAD, EVENT) touchpad_mapper(JOYPAD, EVENT)

#else

#define ANDROID_INIT_TOUCH_PAD(CTX)
#define ANDROID_FREE_TOUCH_PAD()
#define ANDROID_RENDER_TOUCH_CONTROLS(CTX)
#define ANDROID_TOUCHPAD_MAPPER(JOYPAD, EVENT)

#endif