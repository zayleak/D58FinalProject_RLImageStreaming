#ifndef JITTER_BUFFER_H
#define JITTER_BUFFER_H

#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include "rtp.h" 

#define JITTER_BUFFER_SIZE 50
#define JITTER_DELAY_MS 100  // Hold packets for 100ms before releasing

// Buffered packet with timing info
typedef struct {
    rtp_packet_t packet;
    struct timeval arrival_time;
    size_t packet_size;
    int valid;
} buffered_packet_t;

// Jitter buffer structure
typedef struct {
    buffered_packet_t buffer[JITTER_BUFFER_SIZE];
    int head;  // Next position to write
    int tail;  // Next position to read
    int count; // Number of packets in buffer
} jitter_buffer_t;

// Function Prototypes

// Initialize jitter buffer
void init_jitter_buffer(jitter_buffer_t *jb);

// Add packet to jitter buffer
int jitter_buffer_add(jitter_buffer_t *jb, rtp_packet_t *packet, size_t size);

// Get time difference in milliseconds (Internal helper, exposed for testing/clarity)
long time_diff_ms(struct timeval *start, struct timeval *end);

// Try to get a packet from jitter buffer (if ready)
// Returns: pointer to packet if ready, NULL if should wait longer
rtp_packet_t* jitter_buffer_get(jitter_buffer_t *jb, size_t *size);

// Get number of packets in buffer
int jitter_buffer_count(jitter_buffer_t *jb);

#endif // JITTER_BUFFER_H