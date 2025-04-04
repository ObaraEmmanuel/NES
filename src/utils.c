#include "utils.h"
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <SDL.h>
#include <string.h>

#ifndef PI
# define PI	3.14159265358979323846264338327950288
#endif


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
            case WARN:
                priority = ANDROID_LOG_WARN;
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
            case WARN:
                printf("WARN  > ");
                break;
            default:
                printf("LOG   > ");
        }
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
        fflush(stdout);
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
        status += SDL_RenderPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderPoint(renderer, x - offsety, y - offsetx);

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

        status += SDL_RenderLine(renderer, x - offsety, y + offsetx,
                                     x + offsety, y + offsetx);
        status += SDL_RenderLine(renderer, x - offsetx, y + offsety,
                                     x + offsetx, y + offsety);
        status += SDL_RenderLine(renderer, x - offsetx, y - offsety,
                                     x + offsetx, y - offsety);
        status += SDL_RenderLine(renderer, x - offsety, y - offsetx,
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

    return status;
}

void SDL_PauseAudio(SDL_AudioStream* stream, const int flag) {
    SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(stream);
    int paused = SDL_AudioDevicePaused(dev);
    if(paused == flag)
        return;
    if(flag)
        SDL_PauseAudioDevice(dev);
    else
        SDL_ResumeAudioDevice(dev);
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
                quit(EXIT_FAILURE);
        }
    }
}

void fft(complx *v, int n, complx *tmp) {
    if(n <= 1)
        return;
    /* otherwise, do nothing and return */
    int k, m;
    complx z, w, *vo, *ve;
    ve = tmp;
    vo = tmp + n / 2;
    for (k = 0; k < n / 2; k++) {
        ve[k] = v[2 * k];
        vo[k] = v[2 * k + 1];
    }
    fft(ve, n / 2, v); /* FFT on even-indexed elements of v[] */
    fft(vo, n / 2, v); /* FFT on odd-indexed elements of v[] */
    for (m = 0; m < n / 2; m++) {
        w.Re = cos(2 * PI * m / (double) n);
        w.Im = -sin(2 * PI * m / (double) n);
        z.Re = w.Re * vo[m].Re - w.Im * vo[m].Im; /* Re(w*vo[m]) */
        z.Im = w.Re * vo[m].Im + w.Im * vo[m].Re; /* Im(w*vo[m]) */
        v[m].Re = ve[m].Re + z.Re;
        v[m].Im = ve[m].Im + z.Im;
        v[m + n / 2].Re = ve[m].Re - z.Re;
        v[m + n / 2].Im = ve[m].Im - z.Im;
    }
}

char *get_file_name(char *path) {
    char *pfile = path + strlen(path);
    for (; pfile > path; pfile--){
        if ((*pfile == '\\') || (*pfile == '/')){
            pfile++;
            break;
        }
    }
    return pfile;
}

uint64_t next_power_of_2(uint64_t num) {
    int64_t power = 1;
    while(power < num)
        power*=2;
    return power;
}

void quit(int code) {
#if EXIT_PAUSE
    printf("Press any key to exit . . .");
    getchar();
#endif
    exit(code);
}
