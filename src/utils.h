#pragma once

#include <stdio.h>
#include <stdint.h>

#define LOGLEVEL 1
#define TRACER 0

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
    INFO,
};

size_t file_size(FILE* file);
void LOG(enum LogLevel logLevel, const char* fmt, ...);
void TRACE(const char* fmt, ...);