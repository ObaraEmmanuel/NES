#include <SDL.h>

#include "gamepad.h"
#include "utils.h"
#include "controller.h"

#define JOYSTICK_DEADZONE 10000

static GamePad* game_pads[MAX_PADS];
static int game_pad_c = 0;
static const uint16_t key_map[CONTROLLER_KEY_COUNT] = {
    TURBO_A,
    BUTTON_B,
    TURBO_B,
    BUTTON_A,
    SELECT,
    START,
    START,
    TURBO_A,
    TURBO_B,
    BUTTON_A,
    BUTTON_B,
};

static int pad_index(GamePad* pad);
static int num_gamepads();

void init_pads(){
    for (int i = 0; i < num_gamepads(); i++) {
        GamePad* pad = SDL_OpenGamepad(i);
        if(pad != NULL) {
            game_pads[game_pad_c++] = pad;
            LOG(INFO, "Gamepad connected");
        }
    }
}

static int num_gamepads() {
    int count = 0;
    SDL_GetGamepads(&count);
    return count;
}

void gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event){
    uint16_t key = 0;
    switch (event->type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            if(game_pad_c < MAX_PADS) {
                GamePad* pad = SDL_OpenGamepad(event->gdevice.which);
                if(pad != NULL && pad_index(pad) < 0) {
                    game_pads[game_pad_c++] = pad;
                    LOG(INFO, "Gamepad connected");
                }
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            GamePad* pad = SDL_GetGamepadFromID(event->gdevice.which);
            for(int i = 0; i < game_pad_c; i++){
                if(pad == game_pads[i]){
                    for(int j = i; j < game_pad_c - 1; j++){
                        game_pads[j] = game_pads[j + 1];
                    }
                    game_pads[game_pad_c--] = NULL;
                    SDL_CloseGamepad(pad);
                    LOG(INFO, "Gamepad removed");
                    break;
                }
            }
            break;
        }
        default: {
            GamePad* pad = SDL_GetGamepadFromID(event->gdevice.which);
            if(joyPad->player != pad_index(pad))
                // the joypad is not interested in the active controller's input
                // this check allows management of multiple controllers
                return;
            switch (event->type) {
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                // LOG(DEBUG, "BUTTON: %u", event->gbutton.button);
                    if (event->gbutton.button < CONTROLLER_KEY_COUNT) {
                        key = key_map[event->gbutton.button];
                    }

                    if(event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                        joyPad->status |= key;
                        if(key == TURBO_A) {
                            // set button A
                            joyPad->status |= BUTTON_A;
                        }
                        if(key == TURBO_B) {
                            // set button B
                            joyPad->status |= BUTTON_B;
                        }
                    } else if(event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                        joyPad->status &= ~key;
                        if(key == TURBO_A) {
                            // clear button A
                            joyPad->status &= ~BUTTON_A;
                        }
                        if(key == TURBO_B) {
                            // clear button B
                            joyPad->status &= ~BUTTON_B;
                        }
                    }
                    break;
                case SDL_EVENT_JOYSTICK_HAT_MOTION:
                    if (event->jhat.value & SDL_HAT_LEFT)
                        key |= LEFT;
                    else if (event->jhat.value & SDL_HAT_RIGHT)
                        key |= RIGHT;
                    if (event->jhat.value & SDL_HAT_UP)
                        key |= UP;
                    else if (event->jhat.value & SDL_HAT_DOWN)
                        key |= DOWN;

                    if (event->jhat.value == SDL_HAT_CENTERED) {
                        // Reset hat
                        key = RIGHT | LEFT | UP | DOWN;
                        joyPad->status &= ~key;
                    } else {
                        joyPad->status |= key;
                    }
                    break;
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    if (event->gaxis.value < -JOYSTICK_DEADZONE) {
                        if (event->gaxis.axis == 0)
                            key = LEFT;
                        else if (event->gaxis.axis == 1)
                            key = UP;
                        joyPad->status |= key;
                    } else if (event->gaxis.value > JOYSTICK_DEADZONE) {
                        if (event->gaxis.axis == 0)
                            key = RIGHT;
                        else if (event->gaxis.axis == 1)
                            key = DOWN;
                        joyPad->status |= key;
                    } else {
                        // Reset axis
                        if (event->gaxis.axis == 0)
                            key = RIGHT | LEFT;
                        else if (event->gaxis.axis == 1)
                            key = UP | DOWN;

                        joyPad->status &= ~key;
                    }
                    break;
            }
        }
    }
}

static int pad_index(GamePad* pad){
    for(int i = 0; i < game_pad_c; i++){
        if(game_pads[i] == pad)
            return i;
    }
    return -1;
}