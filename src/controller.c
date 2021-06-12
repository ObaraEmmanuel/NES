#include "controller.h"
#include "gamepad.h"
#include "utils.h"


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

uint16_t keyboard_mapper(struct JoyPad* joyPad, SDL_Event* event, uint8_t* type){
    uint16_t key = 0;
    switch (event->key.keysym.sym) {
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
        case SDLK_j:
            key = BUTTON_A;
            break;
        case SDLK_k:
            key = BUTTON_B;
            break;
        case SDLK_l:
            key = TURBO_B;
            break;
        case SDLK_h:
            key = TURBO_A;
            break;

    }
    *type = *type = event->type == SDL_KEYUP ? 1: event->type == SDL_KEYDOWN ? 2: 0;
    return key;
}

void update_joypad(struct JoyPad* joyPad, SDL_Event* event){
    uint8_t type = 0;
    // try the game controller
    uint16_t key = gamepad_mapper(joyPad, event, &type);
    if(!key)
        // try the keyboard
        key = keyboard_mapper(joyPad, event, &type);
    // let handling be done by the turbo buttons
    if(key & (joyPad->status >> 8))
        return;

    if(key){
        if(type == 2){
            joyPad->status |= key;
        } else if(type == 1){
            joyPad->status &= ~key;
        }
    }
}

void turbo_trigger(struct JoyPad* joyPad){
    // toggle BUTTON_A AND BUTTON_B if TURBO_A and TURBO_B are set respectively
    joyPad->status ^= joyPad->status >> 8;
}