#include "utils.h"
#include <stdarg.h>

size_t file_size(FILE* file){
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    return size;
}

void LOG(enum LogLevel logLevel, const char* fmt, ...){
    if(TRACER)
        return;

    if(LOGGER) {
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
    }
}
