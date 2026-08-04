#include "controller.h"
#include "gamepad.h"
#ifdef __ANDROID__
#include "touchpad.h"
#endif


void init_joypad(struct JoyPad* joyPad, uint8_t player){
    joyPad->status = 0;
    joyPad->reg = 0;
    joyPad->player = player;
}

uint8_t read_joypad(struct JoyPad* joyPad){
    uint8_t val = joyPad->reg & 1;
    joyPad->reg >>= 1;
    // refill BIT 7 with 1
    joyPad->reg |= 0x80;
    return val;
}


void keyboard_mapper(struct JoyPad* joyPad, SDL_Event* event){
    uint16_t key = 0;
    switch (event->key.key) {
        case SDLK_RIGHT:
            key = RIGHT;
            break;
        case SDLK_LEFT:
            key = LEFT;
            break;
        case SDLK_DOWN:
            key = DOWN;
            break;
        case SDLK_UP:
            key = UP;
            break;
        case SDLK_RETURN:
            key = START;
            break;
        case SDLK_RSHIFT:
            key = SELECT;
            break;
        case SDLK_J:
            key = BUTTON_A;
            break;
        case SDLK_K:
            key = BUTTON_B;
            break;
        case SDLK_L:
            key = TURBO_B;
            break;
        case SDLK_H:
            key = TURBO_A;
            break;

    }
    if(event->type == SDL_EVENT_KEY_UP) {
        joyPad->status &= ~key;
        if(key == TURBO_A) {
            // clear button A
            joyPad->status &= ~BUTTON_A;
        }
        if(key == TURBO_B) {
            // clear button B
            joyPad->status &= ~BUTTON_B;
        }
    } else if(event->type == SDL_EVENT_KEY_DOWN) {
        joyPad->status |= key;
        if(key == TURBO_A) {
            // set button A
            joyPad->status |= BUTTON_A;
        }
        if(key == TURBO_B) {
            // set button B
            joyPad->status |= BUTTON_B;
        }
    }
}

void update_joypad(struct JoyPad* joyPad, SDL_Event* event){
#ifdef __ANDROID__
    ANDROID_TOUCHPAD_MAPPER(joyPad, event);
#endif
    keyboard_mapper(joyPad, event);
    gamepad_mapper(joyPad, event);
}

void turbo_trigger(struct JoyPad* joyPad){
    // toggle BUTTON_A AND BUTTON_B if TURBO_A and TURBO_B are set respectively
    joyPad->status ^= joyPad->status >> 8;
}