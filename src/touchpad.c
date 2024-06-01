#ifdef __ANDROID__
#include <stdlib.h>
#include <SDL2/SDL_ttf.h>

#include "utils.h"
#include "touchpad.h"

static TouchPad touch_pad;

enum{
    BUTTON_CIRCLE,
    BUTTON_LONG
};

static void init_button(struct TouchButton* button, KeyPad id, size_t index, int type, char* label, int x, int y);
static void init_axis(GraphicsContext* ctx, int x, int y);
static uint8_t is_within_bound(int eventX, int eventY, SDL_Rect* bound);
static uint8_t is_within_circle(int eventX, int eventY, int centerX, int centerY, int radius);
static void to_abs_position(double* x, double* y);
static void update_joy_pos();

void init_touch_pad(GraphicsContext* ctx){
    touch_pad.g_ctx = ctx;

    ctx->font = TTF_OpenFont("asap.ttf", (int)(ctx->screen_height * 0.05));
    if(ctx->font == NULL){
        LOG(ERROR, SDL_GetError());
    }

    int offset = (int)(0.2 * ctx->screen_height);
    int anchor_y = ctx->screen_height - offset;
    int anchor_x = ctx->screen_width - offset;
    int anchor_mid = ctx->screen_height / 2;

    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);

    init_button(&touch_pad.A, BUTTON_A, 0, BUTTON_CIRCLE, "A", anchor_x, anchor_y - offset/2);
    init_button(&touch_pad.turboA, TURBO_A, 1, BUTTON_CIRCLE, "X",  anchor_x, anchor_y + offset/2);
    init_button(&touch_pad.B, BUTTON_B, 2, BUTTON_CIRCLE, "B",  anchor_x - offset/2, anchor_y);
    init_button(&touch_pad.turboB, TURBO_B, 3, BUTTON_CIRCLE, "Y",  anchor_x + offset/2, anchor_y);
    init_button(&touch_pad.select, SELECT, 4, BUTTON_LONG,"Select",  offset, anchor_mid);
    init_button(&touch_pad.start, START, 5, BUTTON_LONG," Start ",  anchor_x, anchor_mid);

    init_axis(ctx, offset, anchor_y);

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);

    LOG(DEBUG, "Initialized touch controls");
}


static void init_button(struct TouchButton* button, KeyPad id, size_t index, int type, char* label, int x, int y){
    GraphicsContext* ctx = touch_pad.g_ctx;
    touch_pad.buttons[index] = button;
    SDL_Color color = {255, 255, 255};
    SDL_Surface* text_surf = TTF_RenderText_Solid(ctx->font, label, color);
    SDL_Texture* text = SDL_CreateTextureFromSurface(ctx->renderer, text_surf);

    int w = text_surf->w + 50, h = text_surf->h + 30, r = h / 2;
    if(type == BUTTON_CIRCLE)
        w = h;
    SDL_Rect dest;
    button->texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(button->texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ctx->renderer, button->texture);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);
    if(w != h) {
        SDL_RenderDrawCircle(ctx->renderer, r, r, r - 1);
        SDL_RenderDrawCircle(ctx->renderer, w - r, r, r - 1);
        dest.h = h;
        dest.w = w - h;
        dest.x = r;
        dest.y = 0;
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(ctx->renderer, &dest);
        SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);
        SDL_RenderDrawLine(ctx->renderer, r, 1, w - r, 1);
        SDL_RenderDrawLine(ctx->renderer, r, h-1, w - r, h-1);
    }else{
        SDL_RenderDrawCircle(ctx->renderer, r, r, r-1);
    }
    dest.x = (w - text_surf->w) / 2;
    dest.y = (h - text_surf->h) / 2;
    dest.w = text_surf->w;
    dest.h = text_surf->h;
    SDL_RenderCopy(ctx->renderer, text, NULL, &dest);
    SDL_SetRenderTarget(ctx->renderer, NULL);

    button->x = x;
    button->y = y;
    button->dest.x = button->x - w / 2;
    button->dest.y = button->y - h / 2;
    button->dest.w = w;
    button->dest.h = h;
    button->id = id;

    SDL_FreeSurface(text_surf);
    SDL_DestroyTexture(text);
}


static void init_axis(GraphicsContext* ctx, int x, int y){
    TouchAxis* axis = &touch_pad.axis;
    axis->r = (int)(0.09 * ctx->screen_height);
    axis->x = axis->inner_x = x;
    axis->y = axis->inner_y = y;

    int bg_r = axis->r + 30;
    axis->bg_dest.x = axis->x - bg_r;
    axis->bg_dest.y = axis->y - bg_r;
    axis->bg_dest.w = axis->bg_dest.h = bg_r * 2;
    axis->joy_dest.w = axis->joy_dest.h = axis->r * 2;
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);
    axis->bg_tx = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, bg_r * 2, bg_r * 2);
    axis->joy_tx = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, axis->r * 2, axis->r * 2);
    SDL_SetTextureBlendMode(axis->bg_tx, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(axis->joy_tx, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ctx->renderer, axis->bg_tx);
    SDL_RenderDrawCircle(ctx->renderer, bg_r, bg_r, bg_r - 2);
    SDL_SetRenderTarget(ctx->renderer, axis->joy_tx);
    SDL_RenderFillCircle(ctx->renderer, axis->r, axis->r, axis->r - 2);
    SDL_SetRenderTarget(ctx->renderer, NULL);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
}


static void update_joy_pos(){
    TouchAxis* axis = &touch_pad.axis;
    axis->joy_dest.x = axis->inner_x - axis->r;
    axis->joy_dest.y = axis->inner_y - axis->r;
}


void render_touch_controls(GraphicsContext* ctx){
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);

    // render axis
    if(!touch_pad.axis.active){
        touch_pad.axis.inner_x = touch_pad.axis.x;
        touch_pad.axis.inner_y = touch_pad.axis.y;
        update_joy_pos();
    }
    SDL_RenderCopy(ctx->renderer, touch_pad.axis.bg_tx, NULL, &touch_pad.axis.bg_dest);
    SDL_RenderCopy(ctx->renderer, touch_pad.axis.joy_tx, NULL, &touch_pad.axis.joy_dest);

    for(int i = 0; i < TOUCH_BUTTON_COUNT; i++){
        TouchButton* button = touch_pad.buttons[i];
        SDL_RenderCopy(ctx->renderer, button->texture, NULL, &button->dest);
    }
}


void touchpad_mapper(struct JoyPad* joyPad, SDL_Event* event){
    // single player support
    if(joyPad->player != 0){
        return;
    }
    uint16_t key = joyPad->status;
    double x, y;
    switch (event->type) {
        case SDL_FINGERUP: {
            x = event->tfinger.x;
            y = event->tfinger.y;
            to_abs_position(&x, &y);
            for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
                TouchButton *button = touch_pad.buttons[i];
                if (button->active && button->finger == event->tfinger.fingerId) {
                    button->active = 0;
                    button->finger = -1;
                    // type as button release
                    key &= ~button->id;
                    LOG(DEBUG, "Released button finger id: %d", event->tfinger.fingerId);
                }
            }

            // handle axis
            TouchAxis *axis = &touch_pad.axis;
            if (axis->finger == event->tfinger.fingerId && axis->active) {
                axis->active = 0;
                axis->finger = -1;
                // reset all axis ids
                axis->v_latch = axis->h_latch = 0;
                key &= ~(RIGHT | LEFT | UP | DOWN);
                LOG(DEBUG, "Reset axis");
            }

            break;
        }
        case SDL_FINGERDOWN: {
            x = event->tfinger.x;
            y = event->tfinger.y;
            to_abs_position(&x, &y);
            for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
                TouchButton *button = touch_pad.buttons[i];
                uint8_t has_event;
                if (i < 4)
                    has_event = is_within_circle((int) x, (int) y, button->x, button->y,
                                                 button->dest.h / 2 + 30);
                else
                    has_event = is_within_bound((int) x, (int) y, &button->dest);

                if (has_event) {
                    button->active = 1;
                    button->finger = event->tfinger.fingerId;
                    // type as button press
                    LOG(DEBUG, "Button pressed");
                    key |= button->id;
                }
            }

            // handle axis
            TouchAxis *axis = &touch_pad.axis;
            if (is_within_circle((int) x, (int) y, axis->x, axis->y, axis->r)) {
                axis->active = 1;
                axis->finger = event->tfinger.fingerId;
            }

            break;
        }
        case SDL_FINGERMOTION: {
            // motion deltas (change in position)
            x = event->tfinger.dx;
            y = event->tfinger.dy;
            to_abs_position(&x, &y);

            TouchAxis *axis = &touch_pad.axis;
            if (axis->active && axis->finger == event->tfinger.fingerId) {
                int new_x = axis->inner_x + (int) x;
                int new_y = axis->inner_y + (int) y;
                int d_x = axis->x - new_x, d_y = axis->y - new_y, r = axis->r / 2, thresh = axis->r / 3;

                if (abs(d_x) > r)
                    new_x = axis->x - (d_x / abs(d_x)) * r;
                if (abs(d_y) > r)
                    new_y = axis->y - (d_y / abs(d_y)) * r;

                axis->inner_x = new_x;
                axis->inner_y = new_y;
                update_joy_pos();

                d_x = axis->x - new_x;
                d_y = axis->y - new_y;

                // press
                if (abs(d_y) > thresh) {
                    if (!axis->v_latch) {
                        axis->v_latch = 1;
                        if (d_y > 0)
                            key |= UP;
                        else
                            key |= DOWN;
                    }
                } else if (abs(d_x) > thresh) {
                    if (!axis->h_latch) {
                        axis->h_latch = 1;
                        if (d_x > 0)
                            key |= LEFT;
                        else
                            key |= RIGHT;
                    }
                } else {
                    // reset horizontal axis
                    if (axis->h_latch) {
                        LOG(DEBUG, "reset h axis");
                        axis->h_latch = 0;
                        key &= ~(RIGHT | LEFT);
                    }
                    // reset vertical axis
                    else if(axis->v_latch) {
                        axis->v_latch = 0;
                        key &= ~(UP | DOWN);
                        LOG(DEBUG, "reset v axis");
                    }
                }
            }
        }
    }
    joyPad->status = key;
}


static void to_abs_position(double* x, double* y){
    *x = *x * (double)touch_pad.g_ctx->screen_width;
    *y = *y * (double)touch_pad.g_ctx->screen_height;
}


static uint8_t is_within_bound(int eventX, int eventY, SDL_Rect* bound){
    return eventX > bound->x && eventX < (bound->x + bound->w) && eventY > bound->y && (eventY < bound->y + bound->h);
}


static uint8_t is_within_circle(int eventX, int eventY, int centerX, int centerY, int radius){
    return abs(centerX - eventX) < radius && abs(centerY - eventY) < radius;
}


void free_touch_pad(){
    for(int i = 0; i < TOUCH_BUTTON_COUNT; i++)
        SDL_DestroyTexture(touch_pad.buttons[i]->texture);
    SDL_DestroyTexture(touch_pad.axis.joy_tx);
    SDL_DestroyTexture(touch_pad.axis.bg_tx);
    LOG(DEBUG, "Touchpad cleanup complete");
}
#endif