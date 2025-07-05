#include <SDL.h>

#include "gamepad.h"
#include "utils.h"
#include "controller.h"

#define JOYSTICK_DEADZONE 10000

static uint32_t index_offset = -1, gamepad_count = 0;
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
    UP,
    DOWN,
    LEFT,
    RIGHT
};

static int num_gamepads();
static void logPadInfo(GamePad* pad);
static int validate_gamepad(GamePad* pad);
static GamePad* open_gamepad(uint32_t index);

static int validate_gamepad(GamePad* pad) {
    SDL_Joystick* stick = SDL_GetGamepadJoystick(pad);
    if (stick == NULL) {
        return 0;
    }
    if((SDL_GetNumJoystickAxes(stick) < 2 && SDL_GetNumJoystickHats(stick) < 1) || SDL_GetNumJoystickButtons(stick) < 2) {
        return 0;
    }
    return 1;
}

static GamePad* open_gamepad(uint32_t index){
    GamePad* pad = SDL_OpenGamepad(index);
    if(pad != NULL) {
        if(!validate_gamepad(pad)) {
            LOG(WARN, "Gamepad (%s) was detected but is not valid", SDL_GetGamepadName(pad));
            SDL_CloseGamepad(pad);
            return NULL;
        }
        LOG(INFO, "Gamepad connected: %s \n Joysticks: %d", SDL_GetGamepadName(pad));
        logPadInfo(pad);
    }
    if(index_offset <= 0 || gamepad_count == 0) {
        // set index offset to 0 for the first gamepad
        index_offset = index % 2;
    }
    return pad;
}

static void logPadInfo(GamePad* pad) {
    SDL_Joystick* stick = SDL_GetGamepadJoystick(pad);
    LOG(
        DEBUG,
        "Gamepad Info: %s \n product: %d \n vendor: %d \n version %d \n path: %s \n player index: %d \n axes: %d \nballs: %d \n hats: %d \n buttons: %d",
        SDL_GetGamepadName(pad),
        SDL_GetGamepadProduct(pad),
        SDL_GetGamepadVendor(pad),
        SDL_GetJoystickProductVersion(stick),
        SDL_GetGamepadPath(pad),
        SDL_GetGamepadPlayerIndex(pad),
        SDL_GetNumJoystickAxes(stick),
        SDL_GetNumJoystickBalls(stick),
        SDL_GetNumJoystickHats(stick),
        SDL_GetNumJoystickButtons(stick)
    );
}

void gamepad_mapper(struct JoyPad* joyPad, SDL_Event* event){
    uint16_t key = 0;
    switch (event->type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            GamePad* pad = SDL_GetGamepadFromID(event->gdevice.which);
            if(pad == NULL) {
                // pad has not been opened yet
                if(open_gamepad(event->gdevice.which))
                    gamepad_count++;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            GamePad* pad = SDL_GetGamepadFromID(event->gdevice.which);
            if(pad != NULL){
                LOG(INFO, "Gamepad removed: %s", SDL_GetGamepadName(pad));
                SDL_CloseGamepad(pad);
                gamepad_count--;
            }
            break;
        }
        default: {
            GamePad* pad = SDL_GetGamepadFromID(event->gdevice.which);
            if(pad == NULL) {
                return;
            }

            if(joyPad->player != (event->gdevice.which + index_offset) % 2 && gamepad_count > 1)
                // the joypad is not interested in the active controller's input
                // this check allows management of multiple controllers
                return;
            switch (event->type) {
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    if(event->gbutton.button)
                     LOG(DEBUG, "PAD: %s, BUTTON: %u", SDL_GetGamepadName(pad), event->gbutton.button);
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
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
