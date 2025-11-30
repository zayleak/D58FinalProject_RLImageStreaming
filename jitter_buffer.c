#include <stdio.h>
#include <string.h>
#include <time.h>     // Required for clock_gettime
#include <sys/time.h> // Keep for struct timeval definition
#include "jitter_buffer.h"
#include "time_utils.h"

// Initialize jitter buffer
void init_jitter_buffer(jitter_buffer_t *jb) {
    memset(jb, 0, sizeof(jitter_buffer_t));
    jb->head = 0;
    jb->tail = 0;
    jb->count = 0;
    
    for (int i = 0; i < JITTER_BUFFER_SIZE; i++) {
        jb->buffer[i].valid = 0;
        jb->buffer[i].packet_size = 0;
        jb->buffer[i].arrival_time.tv_sec = 0;
        jb->buffer[i].arrival_time.tv_usec = 0;
    }
}

// Add packet to jitter buffer
int jitter_buffer_add(jitter_buffer_t *jb, rtp_packet_t *packet, size_t size) {
    if (jb->count >= JITTER_BUFFER_SIZE) {
        printf("Warning: Jitter buffer full!\n");
        return -1;
    }
    
    int current_index = jb->head; // Save index to print correctly later

    // Store packet
    memcpy(&jb->buffer[current_index].packet, packet, size);
    
    get_monotonic_time(&jb->buffer[current_index].arrival_time);
    
    jb->buffer[current_index].packet_size = size;
    jb->buffer[current_index].valid = 1;
    
    // Move head forward
    jb->head = (jb->head + 1) % JITTER_BUFFER_SIZE;
    jb->count++;
    
    // Debug print (Fixed to print the packet we actually just added)
    // Note: %ld for tv_sec might warn on some systems, cast to (long) is safer
    printf("Added packet to Jitter Buffer. Current Count: %d, arrival_time: %ld.%06ld\n", 
           jb->count, 
           (long)jb->buffer[current_index].arrival_time.tv_sec,
           (long)jb->buffer[current_index].arrival_time.tv_usec);
    
    return 0;
}

// Try to get a packet from jitter buffer (if ready)
rtp_packet_t* jitter_buffer_get(jitter_buffer_t *jb, size_t *size) {
    if (jb->count == 0) {
        return NULL;  // Buffer empty
    }

    // Get current monotonic time to compare
    struct timeval now;
    get_monotonic_time(&now);

    long elapsed = time_diff_ms(&jb->buffer[jb->tail].arrival_time, &now);

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