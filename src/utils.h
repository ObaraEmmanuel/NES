#pragma once

#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef float real;
typedef struct{real Re; real Im;} complx;

#if defined(_WIN32) || defined(_WIN64)
#define _WIN 1
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifdef __ANDROID__

#include <android/log.h>
#define TAG "NES_EMULATOR"
#define PRINTF(FMT, ...) __android_log_print(ANDROID_LOG_DEBUG, FMT, TAG, __VA_ARGS__)

#else

#define PRINTF(...) printf(__VA_ARGS__)

#endif

#define LOGLEVEL 1
#define TRACER 0
#define PROFILE 0
#define PROFILE_STOP_FRAME 1

enum {
    BIT_7 = 1<<7,
    BIT_6 = 1<<6,
    BIT_5 = 1<<5,
    BIT_4 = 1<<4,
    BIT_3 = 1<<3,
    BIT_2 = 1<<2,
    BIT_1 = 1<<1,
    BIT_0 = 1
};

enum LogLevel{
    DEBUG = 0,
    ERROR,
    WARN,
    INFO,
};

size_t file_size(FILE* file);
void LOG(enum LogLevel logLevel, const char* fmt, ...);
void TRACE(const char* fmt, ...);

// midpoint circle algorithm rendering utils
int SDL_RenderDrawCircle(SDL_Renderer * renderer, int x, int y, int radius);
int SDL_RenderFillCircle(SDL_Renderer * renderer, int x, int y, int radius);
void to_pixel_format(const uint32_t* restrict in, uint32_t* restrict out, size_t size, uint32_t format);
void fft(complx *v, int n, complx *tmp);
