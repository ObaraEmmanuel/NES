#include "controller.h"


void init_joypad(struct JoyPad* joyPad){
    joyPad->strobe = 0;
    joyPad->index = 0;
    joyPad->status = 0;
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

void update_joypad(struct JoyPad* joyPad, SDL_Event* event){
    uint8_t key = 0;
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
    }
    if(key){
        if(event->type == SDL_KEYDOWN){
            joyPad->status |= key;
        } else if(event->type == SDL_KEYUP){
            joyPad->status &= ~key;
        }
    }
}
