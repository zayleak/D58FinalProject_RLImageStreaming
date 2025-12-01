#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "stats.h"

#define REORDER_BUFFER_SIZE 101 
#define NEXT_PACKET_WAIT_MS 15


typedef struct {
    uint16_t seq;           
    uint8_t *data;          
    size_t size;         
    int valid;              

} packet_slot_t;

// Reorder buffer structure
typedef struct {
    packet_slot_t slots[REORDER_BUFFER_SIZE];
    uint16_t expected_seq;  
    int initialized;        
    struct timeval packet_wait_time; 
} reorder_buffer_t;

void init_reorder_buffer(reorder_buffer_t *buffer);

void free_reorder_buffer(reorder_buffer_t *buffer);

int insert_packet(reorder_buffer_t *buffer, uint16_t seq, uint8_t *data, size_t size);

uint8_t* get_next_packet(reorder_buffer_t *buffer, size_t *size, stats_t *stats);

#endif // REORDER_BUFFER_H