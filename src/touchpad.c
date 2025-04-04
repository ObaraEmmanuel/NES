#ifdef __ANDROID__
#include <stdlib.h>
#include <math.h>
#include <SDL_ttf.h>

#include "utils.h"
#include "touchpad.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

static TouchPad touch_pad;

enum{
    BUTTON_CIRCLE,
    BUTTON_LONG
};

static void init_button(struct TouchButton* button, KeyPad id, size_t index, int type, char* label, int x, int y, TTF_Font* font);
static void init_axis(GraphicsContext* ctx, int x, int y);
static uint8_t is_within_bound(int eventX, int eventY, SDL_FRect* bound);
static uint8_t is_within_circle(int eventX, int eventY, int centerX, int centerY, int radius);
static void to_abs_position(double* x, double* y);
static int angle(int x1, int y1, int x2, int y2);
static void update_joy_pos();

void init_touch_pad(GraphicsContext* ctx){
    touch_pad.g_ctx = ctx;
    int font_size = (int)((ctx->screen_height * 0.08));
    touch_pad.font = TTF_OpenFont("asap.ttf", (font_size * 4)/3);
    if(ctx->font == NULL){
        LOG(ERROR, SDL_GetError());
    }

    int offset = font_size * 3;
    int anchor_y = ctx->screen_height - offset;
    int anchor_x = ctx->screen_width - offset;
    int anchor_mid = (int)(ctx->screen_height * 0.3);

    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);

    init_button(&touch_pad.A, BUTTON_A, 0, BUTTON_CIRCLE, "A", anchor_x, anchor_y - offset/2, touch_pad.font);
    init_button(&touch_pad.turboA, TURBO_A, 1, BUTTON_CIRCLE, "X",  anchor_x, anchor_y + offset/2, touch_pad.font);
    init_button(&touch_pad.B, BUTTON_B, 2, BUTTON_CIRCLE, "B",  anchor_x - offset/2, anchor_y, touch_pad.font);
    init_button(&touch_pad.turboB, TURBO_B, 3, BUTTON_CIRCLE, "Y",  anchor_x + offset/2, anchor_y, touch_pad.font);
    init_button(&touch_pad.select, SELECT, 4, BUTTON_LONG,"Select",  offset, anchor_mid, ctx->font);
    init_button(&touch_pad.start, START, 5, BUTTON_LONG," Start ",  anchor_x, anchor_mid, ctx->font);

    init_axis(ctx, offset, anchor_y);

    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);

    LOG(DEBUG, "Initialized touch controls");
}


static void init_button(struct TouchButton* button, KeyPad id, size_t index, int type, char* label, int x, int y, TTF_Font* font){
    GraphicsContext* ctx = touch_pad.g_ctx;
    touch_pad.buttons[index] = button;
    SDL_Color color = {0xF9, 0x58, 0x1A};
    SDL_Surface* text_surf = TTF_RenderText_Solid(font, label, 0, color);
    SDL_Texture* text = SDL_CreateTextureFromSurface(ctx->renderer, text_surf);

    int w = text_surf->w + 50, h = text_surf->h + 30, r = h / 2;
    if(type == BUTTON_CIRCLE)
        w = h;
    SDL_FRect dest;
    button->texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(button->texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ctx->renderer, button->texture);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(ctx->renderer);
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 100);
    if(w != h) {
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 220);
        SDL_RenderDrawCircle(ctx->renderer, r, r, r - 1);
        SDL_RenderDrawCircle(ctx->renderer, w - r, r, r - 1);
        dest.h = h;
        dest.w = w - h;
        dest.x = r;
        dest.y = 0;
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 0);
        SDL_RenderFillRect(ctx->renderer, &dest);
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 220);
        SDL_RenderLine(ctx->renderer, r, 1, w - r, 1);
        SDL_RenderLine(ctx->renderer, r, h-1, w - r, h-1);
    }else{
        SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 100);
        SDL_RenderDrawCircle(ctx->renderer, r, r, r-1);
        SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0xF9, 0xF9, 70);
        SDL_RenderFillCircle(ctx->renderer, r, r, r-2);
    }
    button->r = r-1;
    dest.x = (w - text_surf->w) / 2;
    dest.y = (h - text_surf->h) / 2;
    dest.w = text_surf->w;
    dest.h = text_surf->h;
    SDL_RenderTexture(ctx->renderer, text, NULL, &dest);
    SDL_SetRenderTarget(ctx->renderer, NULL);

    button->x = x;
    button->y = y;
    button->dest.x = button->x - w / 2;
    button->dest.y = button->y - h / 2;
    button->dest.w = w;
    button->dest.h = h;
    button->id = id;

    SDL_DestroySurface(text_surf);
    SDL_DestroyTexture(text);
}


static void init_axis(GraphicsContext* ctx, int x, int y){
    TouchAxis* axis = &touch_pad.axis;
    axis->r = (int)(0.09 * ctx->screen_height);
    axis->x = axis->inner_x = axis->origin_x = x;
    axis->y = axis->inner_y = axis->origin_y = y;

    int bg_r = axis->r + 30;
    axis->bg_dest.x = axis->x - bg_r;
    axis->bg_dest.y = axis->y - bg_r;
    axis->bg_dest.w = axis->bg_dest.h = bg_r * 2;
    axis->joy_dest.w = axis->joy_dest.h = axis->r * 2;
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 220);
    axis->bg_tx = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, bg_r * 2, bg_r * 2);
    axis->joy_tx = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, axis->r * 2, axis->r * 2);
    SDL_SetTextureBlendMode(axis->bg_tx, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(axis->joy_tx, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(ctx->renderer, axis->bg_tx);
    SDL_RenderDrawCircle(ctx->renderer, bg_r, bg_r, bg_r - 2);
    SDL_SetRenderTarget(ctx->renderer, axis->joy_tx);
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 180);
    SDL_RenderFillCircle(ctx->renderer, axis->r, axis->r, axis->r - 2);
    SDL_SetRenderTarget(ctx->renderer, NULL);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 200);
}


static void update_joy_pos(){
    TouchAxis* axis = &touch_pad.axis;
    axis->joy_dest.x = axis->inner_x - axis->r;
    axis->joy_dest.y = axis->inner_y - axis->r;
    axis->bg_dest.x = axis->x - axis->r - 30;
    axis->bg_dest.y = axis->y - axis->r - 30;
}


void render_touch_controls(GraphicsContext* ctx){
    SDL_SetRenderDrawColor(ctx->renderer, 0xF9, 0x58, 0x1A, 255);

    // render axis
    if(!touch_pad.axis.active){
        touch_pad.axis.inner_x = touch_pad.axis.x;
        touch_pad.axis.inner_y = touch_pad.axis.y;
        update_joy_pos();
    }
    SDL_RenderTexture(ctx->renderer, touch_pad.axis.bg_tx, NULL, &touch_pad.axis.bg_dest);
    SDL_RenderTexture(ctx->renderer, touch_pad.axis.joy_tx, NULL, &touch_pad.axis.joy_dest);

    for(int i = 0; i < TOUCH_BUTTON_COUNT; i++){
        TouchButton* button = touch_pad.buttons[i];
        SDL_RenderTexture(ctx->renderer, button->texture, NULL, &button->dest);
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
        case SDL_EVENT_FINGER_UP: {
            x = event->tfinger.x;
            y = event->tfinger.y;
            to_abs_position(&x, &y);
            for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
                TouchButton *button = touch_pad.buttons[i];
                if (button->active && button->finger == event->tfinger.fingerID) {
                    button->active = 0;
                    button->finger = -1;
                    // type as button release
                    key &= ~button->id;
                    LOG(DEBUG, "Released button finger id: %d", event->tfinger.fingerID);
                    if(button->id == TURBO_A) {
                        // clear button A
                        key &= ~BUTTON_A;
                    }
                    if(button->id == TURBO_B) {
                        // clear button B
                        key &= ~BUTTON_B;
                    }
                }
            }

            // handle axis
            TouchAxis *axis = &touch_pad.axis;
            if (axis->finger == event->tfinger.fingerID && axis->active) {
                axis->active = 0;
                axis->finger = -1;
                // reset all axis ids
                axis->v_latch = axis->h_latch = 0;
                key &= ~(RIGHT | LEFT | UP | DOWN);
                axis->inner_x = axis->x = axis->origin_x;
                axis->inner_y = axis->y = axis->origin_y;
                LOG(DEBUG, "Reset axis");
            }

            break;
        }
        case SDL_EVENT_FINGER_DOWN: {
            x = event->tfinger.x;
            y = event->tfinger.y;
            to_abs_position(&x, &y);
            int was_button_pressed = 0;
            for (int i = 0; i < TOUCH_BUTTON_COUNT; i++) {
                TouchButton *button = touch_pad.buttons[i];
                uint8_t has_event;
                if (i < 4)
                    has_event = is_within_circle((int) x, (int) y, button->x, button->y,
                                                 button->r);
                else
                    has_event = is_within_bound((int) x, (int) y, &button->dest);

                if (has_event) {
                    was_button_pressed = 1;
                    button->active = 1;
                    button->finger = event->tfinger.fingerID;
                    // type as button press
                    LOG(DEBUG, "Button pressed");
                    key |= button->id;
                    if(button->id == TURBO_A) {
                        // set button A
                        key |= BUTTON_A;
                    }
                    if(key == TURBO_B) {
                        // set button B
                        key |= BUTTON_B;
                    }
                }
            }
            if(was_button_pressed)
                break;

            // handle axis
            TouchAxis *axis = &touch_pad.axis;
            if (x < touch_pad.g_ctx->screen_width / 2 ){
                // left side of the screen
                axis->active = 1;
                axis->finger = event->tfinger.fingerID;
                axis->x = axis->inner_x = (int) x;
                axis->y = axis->inner_y = (int) y;
            }

            break;
        }
        case SDL_EVENT_FINGER_MOTION: {
            TouchAxis *axis = &touch_pad.axis;
            if(!axis->active || axis->finger != event->tfinger.fingerID)
                break;

            x = event->tfinger.x;
            y = event->tfinger.y;
            to_abs_position(&x, &y);

            int a = angle(touch_pad.axis.x, touch_pad.axis.y, (int)x, (int)y);
            int d = (int) sqrt(pow(x - axis->x, 2) + pow(y - axis->y, 2));
            int r = axis->r + 30;

            int new_x = (int) x;
            int new_y = (int) y;


            if (d > r) {
                new_x = (int)(r * cos(a * M_PI / 180) + axis->x);
                new_y = (int)(r * sin(a * M_PI / 180) + axis->y);
            }

            axis->inner_x = new_x;
            axis->inner_y = new_y;
            update_joy_pos();

            if (d < axis->r / 2) {
                // delta is too small to be reliable
                key &= ~(RIGHT | LEFT | UP | DOWN);
                break;
            }

            if(a < 60 || a > 300)
                key |= RIGHT;
            else
                key &= ~RIGHT;

            if(a > 30 && a < 150)
                key |= DOWN;
            else
                key &= ~DOWN;

            if(a > 120 && a < 240)
                key |= LEFT;
            else
                key &= ~LEFT;

            if(a > 210 && a < 330)
                key |= UP;
            else
                key &= ~UP;
        }
        default:
            break;
    }
    joyPad->status = key;
}


static void to_abs_position(double* x, double* y){
    *x = *x * (double)touch_pad.g_ctx->screen_width;
    *y = *y * (double)touch_pad.g_ctx->screen_height;
}


static uint8_t is_within_bound(int eventX, int eventY, SDL_FRect* bound){
    return eventX > bound->x && eventX < (bound->x + bound->w) && eventY > bound->y && (eventY < bound->y + bound->h);
}


static uint8_t is_within_circle(int eventX, int eventY, int centerX, int centerY, int radius){
    return abs(centerX - eventX) < radius && abs(centerY - eventY) < radius;
}

static int angle(int x1, int y1, int x2, int y2){
    int a = (int)(atan2(y2 - y1, x2 - x1) * 180 / M_PI);
    return a < 0 ? 360 + a : a;
}


void free_touch_pad(){
    for(int i = 0; i < TOUCH_BUTTON_COUNT; i++)
        SDL_DestroyTexture(touch_pad.buttons[i]->texture);
    SDL_DestroyTexture(touch_pad.axis.joy_tx);
    SDL_DestroyTexture(touch_pad.axis.bg_tx);
    LOG(DEBUG, "Touchpad cleanup complete");
}
#endif