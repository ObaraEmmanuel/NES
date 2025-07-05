#pragma once

#include <SDL.h>

#define CONTROLLER_KEY_COUNT 15

typedef SDL_Gamepad GamePad;
struct JoyPad;

void gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event);
