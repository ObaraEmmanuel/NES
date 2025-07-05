#include "controller.h"
#include "gamepad.h"
#include "touchpad.h"


void init_joypad(struct JoyPad* joyPad, uint8_t player){
    joyPad->strobe = 0;
    joyPad->index = 0;
    joyPad->status = 0;
    joyPad->player = player;
}


uint8_t read_joypad(struct JoyPad* joyPad){
    if(joyPad->index > 7)
        return 1;
    uint8_t val = (joyPad->status & (1 << joyPad->index)) != 0;
    if(!joyPad->strobe)
        joyPad->index++;
    return val;
}


void write_joypad(struct JoyPad* joyPad, uint8_t data){
    joyPad->strobe = data & 1;
    if(joyPad->strobe)
        joyPad->index = 0;
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
#else
    keyboard_mapper(joyPad, event);
#endif
    gamepad_mapper(joyPad, event);
}

void turbo_trigger(struct JoyPad* joyPad){
    // toggle BUTTON_A AND BUTTON_B if TURBO_A and TURBO_B are set respectively
    joyPad->status ^= joyPad->status >> 8;
}