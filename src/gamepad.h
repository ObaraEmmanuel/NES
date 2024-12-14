#pragma once

#include <SDL.h>

#define MAX_PADS 2
#define CONTROLLER_KEY_COUNT 11

typedef SDL_Gamepad GamePad;
struct JoyPad;

void init_pads();
void gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event);
