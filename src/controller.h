#pragma once

#include <stdint.h>
#include <SDL2/SDL.h>

typedef enum KeyPad{
    TURBO_B     = 1 << 9,
    TURBO_A     = 1 << 8,
    RIGHT       = 1 << 7,
    LEFT        = 1 << 6,
    DOWN        = 1 << 5,
    UP          = 1 << 4,
    START       = 1 << 3,
    SELECT      = 1 << 2,
    BUTTON_B    = 1 << 1,
    BUTTON_A    = 1
} KeyPad;

typedef struct JoyPad{
    uint8_t strobe;
    uint8_t index;
    uint16_t status;
} JoyPad;


void init_joypad(struct JoyPad* joyPad);
uint8_t read_joypad(struct JoyPad* joyPad);
void write_joypad(struct JoyPad* joyPad, uint8_t data);
void update_joypad(struct JoyPad* joyPad, SDL_Event* event);
void turbo_trigger(struct JoyPad* joyPad);
