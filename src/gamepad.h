#pragma once

#include <SDL2/SDL.h>

#define MAX_PADS 2
#define CONTROLLER_KEY_COUNT 10

typedef SDL_Joystick GamePad;
struct JoyPad;

void init_pads();
uint16_t gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event, uint8_t* type);
