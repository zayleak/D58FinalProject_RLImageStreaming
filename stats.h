#ifndef STATS_H
#define STATS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>

/**
 * Structure to hold real-time RTP packet and frame statistics.
 */
typedef struct {
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t frames_received;
    uint16_t last_seq;
    uint32_t total_bytes;
    uint32_t retransmit_requests;
    uint32_t packets_reordered;
    struct timeval start_time;
} stats_t;

void init_stats(stats_t *stats);
void update_stats(stats_t *stats, uint16_t seq, size_t bytes);
void print_stats(stats_t *stats);

#endif // STATS_H