#pragma once

#include <stdio.h>
#include <stdint.h>
#include <SDL.h>

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

#define TRACING_LOGS_ENABLED 0

#ifdef DEBUGGING_ENABLED
    #define EXIT_PAUSE 1
    #if TRACING_LOGS_ENABLED
    #define LOGLEVEL 0
    #else
    #define LOGLEVEL 1
#endif
#else
#define LOGLEVEL 2
#define EXIT_PAUSE 0
#endif

#define TRACER 0
#define PROFILE 0
#define PROFILE_STOP_FRAME 1
#define NAMETABLE_MODE 0

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
    TRACE = 0,
    DEBUG,
    ERROR,
    WARN,
    INFO,
};

typedef enum ColorFormat {
    ARGB8888,
    ABGR8888
} ColorFormat;

size_t file_size(FILE* file);
void LOG(enum LogLevel logLevel, const char* fmt, ...);

// midpoint circle algorithm rendering utils
int SDL_RenderDrawCircle(SDL_Renderer * renderer, int x, int y, int radius);
int SDL_RenderFillCircle(SDL_Renderer * renderer, int x, int y, int radius);
void SDL_PauseAudio(SDL_AudioStream* stream, int flag);
void to_pixel_format(const uint32_t* restrict in, uint32_t* restrict out, size_t size, ColorFormat format);
void fft(complx *v, int n, complx *tmp);
uint64_t next_power_of_2(uint64_t num);
char *get_file_name(char *path);
void quit(int code);
