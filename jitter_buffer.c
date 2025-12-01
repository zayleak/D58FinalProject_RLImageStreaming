#include <stdio.h>
#include <string.h>
#include <time.h>    
#include <sys/time.h> 
#include "jitter_buffer.h"
#include "time_utils.h"


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


int jitter_buffer_add(jitter_buffer_t *jb, rtp_packet_t *packet, size_t size) {
    if (jb->count >= JITTER_BUFFER_SIZE) {
        printf("Warning: Jitter buffer full!\n");
        return -1;
    }
    
    int current_index = jb->head; 

    memcpy(&jb->buffer[current_index].packet, packet, size);
    
    get_monotonic_time(&jb->buffer[current_index].arrival_time);
    
    jb->buffer[current_index].packet_size = size;
    jb->buffer[current_index].valid = 1;

    jb->head = (jb->head + 1) % JITTER_BUFFER_SIZE;
    jb->count++;
    
    printf("Added packet to Jitter Buffer. Current Count: %d, arrival_time: %ld.%06ld\n", 
           jb->count, 
           (long)jb->buffer[current_index].arrival_time.tv_sec,
           (long)jb->buffer[current_index].arrival_time.tv_usec);
    
    return 0;
}


rtp_packet_t* jitter_buffer_get(jitter_buffer_t *jb, size_t *size) {
    if (jb->count == 0) {
        return NULL;  // Buffer empty
    }


    struct timeval now;
    get_monotonic_time(&now);

    long elapsed = time_diff_ms(&jb->buffer[jb->tail].arrival_time, &now);

    if (elapsed >= JITTER_DELAY_MS) {
        *size = jb->buffer[jb->tail].packet_size;
        rtp_packet_t *packet = &jb->buffer[jb->tail].packet;
        jb->buffer[jb->tail].valid = 0;
        jb->tail = (jb->tail + 1) % JITTER_BUFFER_SIZE;
        jb->count--;

        return packet;
    }

    return NULL;  
}
