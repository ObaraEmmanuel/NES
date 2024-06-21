#pragma once

#include <stdint.h>
#include "utils.h"

typedef struct Timer{
    void* timer;
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
