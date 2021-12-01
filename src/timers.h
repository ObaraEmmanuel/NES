#pragma once

#include <stdint.h>
#include "utils.h"

#ifdef _WIN
#include <windows.h>
// sleep resolution in milliseconds
// should be a non-zero and non-negative value
#define SLEEP_RESOLUTION_MS 1
#else
#include <unistd.h>
#include "time.h"
#endif


typedef struct Timer{
#ifdef _WIN
    LARGE_INTEGER start, diff;
    LARGE_INTEGER frequency;
    uint64_t period_ms;
#else
    struct timespec start, diff;
    uint64_t clock_res;
    uint64_t period_ns;
#endif
} Timer;


void init_timer(Timer* timer, uint64_t period);
void mark_start(Timer* timer);
void mark_end(Timer* timer);
int adjusted_wait(Timer* timer);
int wait(uint64_t period_ms);
double get_diff_ms(Timer* timer);
void release_timer(Timer* timer);

#ifdef _WIN
void toggle_timer_resolution();
#define TOGGLE_TIMER_RESOLUTION() toggle_timer_resolution()
#else
#define TOGGLE_TIMER_RESOLUTION()
#endif
