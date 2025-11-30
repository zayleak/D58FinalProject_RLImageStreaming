#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <sys/time.h> // Required for struct timeval
#include <time.h>     // Required for struct timespec

void get_monotonic_time(struct timeval *tv);

long time_diff_ms(struct timeval *start, struct timeval *end);

#endif // TIME_UTILS_H