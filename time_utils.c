#define _POSIX_C_SOURCE 200809L
#include "time_utils.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h> // Keep for struct timeval definition

void get_monotonic_time(struct timeval *tv) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
}

long time_diff_ms(struct timeval *start, struct timeval *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long useconds = end->tv_usec - start->tv_usec;

    // (seconds * 1000) + (microseconds / 1000)
    long mtime = (seconds * 1000) + (useconds / 1000);

    return mtime;
}