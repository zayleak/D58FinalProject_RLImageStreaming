#include <stdio.h> // For printf (Warning message)
#include "jitter_buffer.h"

// Initialize jitter buffer
void init_jitter_buffer(jitter_buffer_t *jb) {
    memset(jb, 0, sizeof(jitter_buffer_t));
    jb->head = 0;
    jb->tail = 0;
    jb->count = 0;
}

// Add packet to jitter buffer
int jitter_buffer_add(jitter_buffer_t *jb, rtp_packet_t *packet, size_t size) {
    if (jb->count >= JITTER_BUFFER_SIZE) {
        printf("Warning: Jitter buffer full!\n");
        return -1;
    }
    printf("Added packet to Jitter Buffer. Current Count: %d\n", jb->count);
    // Store packet
    memcpy(&jb->buffer[jb->head].packet, packet, size);
    gettimeofday(&jb->buffer[jb->head].arrival_time, NULL);
    jb->buffer[jb->head].packet_size = size;
    jb->buffer[jb->head].valid = 1;

    // Move head forward (circular buffer logic)
    jb->head = (jb->head + 1) % JITTER_BUFFER_SIZE;
    jb->count++;
    return 0;
}

// Get time difference in milliseconds
long time_diff_ms(struct timeval *start, struct timeval *end) {
    // Convert each timeval to total milliseconds, then subtract
    long start_ms = (long)(start->tv_sec) * 1000L + (long)(start->tv_usec) / 1000L;
    long end_ms = (long)(end->tv_sec) * 1000L + (long)(end->tv_usec) / 1000L;
    printf(" start_time: %ld\n, end_time: %ld\n", start_ms, end_ms);
    return end_ms - start_ms;
}

// Try to get a packet from jitter buffer (if ready)
// Returns: pointer to packet if ready, NULL if should wait longer
rtp_packet_t* jitter_buffer_get(jitter_buffer_t *jb, size_t *size) {
    if (jb->count == 0) {
        return NULL;  // Buffer empty
    }

    // Check if oldest packet has been in buffer long enough
    struct timeval now;
    gettimeofday(&now, NULL);

    long elapsed = time_diff_ms(&jb->buffer[jb->tail].arrival_time, &now);
    printf(" Timestamp elapsed: %ld\n", elapsed);

    if (elapsed >= JITTER_DELAY_MS) {
        // Packet is ready to be released
        *size = jb->buffer[jb->tail].packet_size;
        rtp_packet_t *packet = &jb->buffer[jb->tail].packet;

        // Mark as used and move tail forward (circular buffer logic)
        jb->buffer[jb->tail].valid = 0;
        jb->tail = (jb->tail + 1) % JITTER_BUFFER_SIZE;
        jb->count--;

        return packet;
    }

    return NULL;  // Not ready yet
}

// Get number of packets in buffer
int jitter_buffer_count(jitter_buffer_t *jb) {
    return jb->count;
}