#include "timers.h"
#include "utils.h"

#define G 1000000000L
#define M 1000000L

#ifdef WIN

void init_timer(Timer* timer, uint64_t period){

}

void mark_start(Timer* timer){

}

void mark_end(Timer* timer){

}

int adjusted_wait(Timer* timer){
    return 0
}


int wait(uint64_t period_ms){
    return 0;
}


double get_diff_ms(Timer* timer){
    return 0;
}

#else // linux

static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result);

void init_timer(Timer* timer, uint64_t period){
    timer->period_ns = period;
    struct timespec res;
    clock_getres(CLOCK_MONOTONIC, &res);
    timer->clock_res = res.tv_sec * G + res.tv_nsec;
    LOG(INFO, "Clock resolution: %lu ns", timer->clock_res);
}


void mark_start(Timer* timer){
    clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

void mark_end(Timer* timer){
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&end, &timer->start, &timer->diff);
}

int adjusted_wait(Timer* timer){
    int64_t req_period_ns = (int64_t)(timer->period_ns - (timer->diff.tv_sec * G + timer->diff.tv_nsec));
    if(req_period_ns <= timer->clock_res)
        return 0;

    struct timespec req = {
            .tv_sec=req_period_ns / G,
            .tv_nsec=req_period_ns % G
    };
    return clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
}


int wait(uint64_t period_ms){
    int64_t req_period_ns = (int64_t)period_ms * M;
    struct timespec req = {
        .tv_sec=req_period_ns / G,
        .tv_nsec=req_period_ns % G
    };
    return clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
}


double get_diff_ms(Timer* timer){
    return ((double)timer->diff.tv_sec * 1000 + (double)timer->diff.tv_nsec / M);
}


static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result) {
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

#endif // linux