#pragma once

#include <stdint.h>

#ifdef _WIN
#include <windows.h>
#else
#include <unistd.h>
#include "time.h"
#endif


typedef struct Timer{
#ifdef _WIN
    LARGE_INTEGER start, diff;
    LARGE_INTEGER period_ns;
#else
    struct timespec start, diff;
    uint64_t period_ns, clock_res;
#endif
} Timer;


void init_timer(Timer* timer, uint64_t period);
void mark_start(Timer* timer);
void mark_end(Timer* timer);
int adjusted_wait(Timer* timer);
int wait(uint64_t period_ms);
double get_diff_ms(Timer* timer);
