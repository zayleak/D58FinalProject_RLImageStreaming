#include "reorder_buffer.h"
#include <stdio.h> // For printf (logging/warnings)
#include "time_utils.h"

// Initialize reorder buffer and allocate memory for slots
void init_reorder_buffer(reorder_buffer_t *buffer) {
    memset(buffer, 0, sizeof(reorder_buffer_t));
    buffer->initialized = 0;
    get_monotonic_time(&buffer->packet_wait_time);

    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
    
        buffer->slots[i].data = (uint8_t*)malloc(2000);
        buffer->slots[i].valid = 0;
        if (!buffer->slots[i].data) {
            fprintf(stderr, "Error: Failed to allocate memory for reorder buffer slot %d\n", i);
        }
    }
}

void free_reorder_buffer(reorder_buffer_t *buffer) {
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        if (buffer->slots[i].data) {
            free(buffer->slots[i].data);
            buffer->slots[i].data = NULL; 
        }
    }
}

int insert_packet(reorder_buffer_t *buffer, uint16_t seq, uint8_t *data, size_t size) {
    if (!buffer->initialized) {
        buffer->expected_seq = seq;
        buffer->initialized = 1;
    }

    int16_t offset = (int16_t)(seq - buffer->expected_seq);
    if (offset < 0) {
        printf("Ignoring old packet: seq=%u (expected=%u)\n", seq, buffer->expected_seq);
        return 0;
    }
    else if (offset >= REORDER_BUFFER_SIZE) {
   
        printf("Warning: Packet too far ahead, buffer full moving reorder buffer (seq=%u, expected=%u)\n",
                seq, buffer->expected_seq);
        
        return 0;
    }
        
    int slot_index = offset;

    if (buffer->slots[slot_index].valid) {
        return 0;
    }

    buffer->slots[slot_index].seq = seq;
    memcpy(buffer->slots[slot_index].data, data, size);
    buffer->slots[slot_index].size = size;
    buffer->slots[slot_index].valid = 1;

    if (offset > 0) {
        printf("Buffered out-of-order packet: seq=%u at slot %d (expected=%u)\n",
            seq, slot_index, buffer->expected_seq);
        return 1;
    }

    return 0;
}

uint8_t* shift_seq(reorder_buffer_t *buffer) {
    buffer->expected_seq++;

    uint8_t* data_to_return = buffer->slots[0].data; 
    for (int i = 0; i < REORDER_BUFFER_SIZE - 1; i++) {
        buffer->slots[i] = buffer->slots[i + 1];
    }

    buffer->slots[REORDER_BUFFER_SIZE - 1].data = data_to_return;
    buffer->slots[REORDER_BUFFER_SIZE - 1].valid = 0;
    buffer->slots[REORDER_BUFFER_SIZE - 1].seq = 0;
    buffer->slots[REORDER_BUFFER_SIZE - 1].size = 0;
    get_monotonic_time(&buffer->packet_wait_time);
    return data_to_return;
}


uint8_t* get_next_packet(reorder_buffer_t *buffer, size_t *size, stats_t *stats) {

    if (buffer->slots[0].valid && buffer->slots[0].seq == buffer->expected_seq) {
        *size = buffer->slots[0].size;
        return shift_seq(buffer);;
    }

    struct timeval now;
    get_monotonic_time(&now);
    long elapsed = time_diff_ms(&buffer->packet_wait_time, &now);
    if (elapsed > NEXT_PACKET_WAIT_MS) {
        if (stats != NULL) {
            stats->packets_lost++;
        }
        shift_seq(buffer);
        return get_next_packet(buffer, size, stats);
    }

    return NULL; 
}
