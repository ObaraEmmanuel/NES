#include <SDL2/SDL.h>

#include "gamepad.h"
#include "utils.h"
#include "controller.h"

static GamePad* game_pads[MAX_PADS];
static int game_pad_c = 0;
static const uint16_t key_map[CONTROLLER_KEY_COUNT] = {
    TURBO_A,
    BUTTON_B,
    TURBO_B,
    BUTTON_A,
    BUTTON_A,
    BUTTON_B,
    TURBO_A,
    TURBO_B,
    SELECT,
    START,
};

static int pad_index(GamePad* pad);

void init_pads(){
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        GamePad* pad = SDL_JoystickOpen(i);
        if(pad != NULL) {
            game_pads[game_pad_c++] = pad;
            LOG(INFO, "Joypad connected");
        }
    }
}

uint16_t gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event, uint8_t* type){
    uint16_t key = 0;
    switch (event->type) {
        case SDL_JOYDEVICEADDED: {
            if(game_pad_c < MAX_PADS) {
                GamePad* pad = SDL_JoystickOpen(event->jdevice.which);
                if(pad != NULL && pad_index(pad) < 0) {
                    game_pads[game_pad_c++] = pad;
                    LOG(INFO, "Joypad connected");
                }
            }
            break;
        }
        case SDL_JOYDEVICEREMOVED: {
            GamePad* pad = SDL_JoystickFromInstanceID(event->jdevice.which);
            for(int i = 0; i < game_pad_c; i++){
                if(pad == game_pads[i]){
                    for(int j = i; j < game_pad_c - 1; j++){
                        game_pads[j] = game_pads[j + 1];
                    }
                    game_pads[game_pad_c--] = NULL;
                    SDL_JoystickClose(pad);
                    LOG(INFO, "Joypad removed");
                    break;
                }
            }
            break;
        }
        default: {
            GamePad* pad = SDL_JoystickFromInstanceID(event->jdevice.which);
            if(joyPad->player != pad_index(pad))
                // the joypad is not interested in the active controller's input
                // this check allows management of multiple controllers
                return 0;

            switch (event->type) {
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                    if (event->jbutton.button >= 0 && event->jbutton.button < 10) {
                        key = key_map[event->jbutton.button];
                    }
                    *type = *type = event->type == SDL_JOYBUTTONUP ? 1 : event->type == SDL_JOYBUTTONDOWN ? 2 : 0;
                    break;
                case SDL_JOYHATMOTION:
                    if (event->jhat.value & SDL_HAT_LEFT)
                        key |= LEFT;
                    if (event->jhat.value & SDL_HAT_RIGHT)
                        key |= RIGHT;
                    if (event->jhat.value & SDL_HAT_UP)
                        key |= UP;
                    if (event->jhat.value & SDL_HAT_DOWN)
                        key |= DOWN;
                    *type = 2;

                    if (event->jhat.value == SDL_HAT_CENTERED) {
                        // Reset hat
                        key = RIGHT | LEFT | UP | DOWN;
                        *type = 1;
                    }
                    break;
                case SDL_JOYAXISMOTION:
                    *type = 2;
                    if (event->jaxis.value < -3200) {
                        if (event->jaxis.axis == 0)
                            key = LEFT;
                        else if (event->jaxis.axis == 1)
                            key = UP;
                    } else if (event->jaxis.value > 3200) {
                        if (event->jaxis.axis == 0)
                            key = RIGHT;
                        else if (event->jaxis.axis == 1)
                            key = DOWN;
                    } else {
                        // Reset axis
                        if (event->jaxis.axis == 0)
                            key = RIGHT | LEFT;
                        else if (event->jaxis.axis == 1)
                            key = UP | DOWN;

                        *type = 1;
                    }
                    break;
            }
        }
    }
    return key;
}

static int pad_index(GamePad* pad){
    for(int i = 0; i < game_pad_c; i++){
        if(game_pads[i] == pad)
            return i;
    }
    return -1;
}