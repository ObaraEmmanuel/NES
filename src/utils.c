#include "utils.h"
#include <stdarg.h>
#include <SDL2/SDL.h>

size_t file_size(FILE* file){
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    return size;
}

void LOG(enum LogLevel logLevel, const char* fmt, ...){
    if(TRACER)
        return;
    if(logLevel >= LOGLEVEL) {
#ifdef __ANDROID__
        android_LogPriority priority;
        switch (logLevel) {
            case INFO:
                priority = ANDROID_LOG_INFO;
                break;
            case DEBUG:
                priority = ANDROID_LOG_DEBUG;
                break;
            case ERROR:
                priority = ANDROID_LOG_ERROR;
                break;
            default:
                priority = ANDROID_LOG_UNKNOWN;
        }
        va_list ap;
        va_start(ap, fmt);
        __android_log_vprint(priority, TAG, fmt, ap);
        va_end(ap);
#else
        switch (logLevel) {
            case INFO:
                printf("INFO  > ");
                break;
            case DEBUG:
                printf("DEBUG > ");
                break;
            case ERROR:
                printf("ERROR > ");
                break;
            default:
                printf("LOG   > ");
        }
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
#endif
    }
}

int SDL_RenderDrawCircle(SDL_Renderer * renderer, int x, int y, int radius){
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius -1;
    status = 0;

    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2*offsetx) {
            d -= 2*offsetx + 1;
            offsetx +=1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}


int SDL_RenderFillCircle(SDL_Renderer * renderer, int x, int y, int radius) {
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
                                     x + offsety, y + offsetx);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
                                     x + offsetx, y + offsety);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
                                     x + offsetx, y - offsety);
        status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
                                     x + offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        } else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        } else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }
}


void to_pixel_format(const uint32_t* restrict in, uint32_t* restrict out, size_t size, uint32_t format){
    for(int i = 0; i < size; i++) {
        switch (format) {
            case SDL_PIXELFORMAT_ARGB8888:{
                out[i] = in[i];
                break;
            }
            case SDL_PIXELFORMAT_ABGR8888:{
                out[i] = (in[i] & 0xff000000) | ((in[i] << 16) & 0x00ff0000) | (in[i] & 0x0000ff00) | ((in[i] >> 16) & 0x000000ff);
                break;
            }
            default:
                LOG(DEBUG, "Unsupported format");
                exit(EXIT_FAILURE);
        }
    }
}
