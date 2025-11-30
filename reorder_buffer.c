#include "reorder_buffer.h"
#include <stdio.h> // For printf (logging/warnings)
#include "time_utils.h"

// Initialize reorder buffer and allocate memory for slots
void init_reorder_buffer(reorder_buffer_t *buffer) {
    memset(buffer, 0, sizeof(reorder_buffer_t));
    buffer->initialized = 0;
    get_monotonic_time(&buffer->packet_wait_time);

    // Allocate memory for each slot's payload buffer
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        // Allocate a buffer to hold the payload data
        buffer->slots[i].data = (uint8_t*)malloc(2000);  // Assuming max 2000 byte packet payload
        buffer->slots[i].valid = 0;
        if (!buffer->slots[i].data) {
            fprintf(stderr, "Error: Failed to allocate memory for reorder buffer slot %d\n", i);
            // In a real app, proper error handling would be required here.
        }
    }
}

// Free reorder buffer (freeing allocated memory for payloads)
void free_reorder_buffer(reorder_buffer_t *buffer) {
    for (int i = 0; i < REORDER_BUFFER_SIZE; i++) {
        if (buffer->slots[i].data) {
            free(buffer->slots[i].data);
            buffer->slots[i].data = NULL; // Prevent double free
        }
    }
}

// Insert packet into reorder buffer
// Returns: 1 if this is the expected packet (in order), 0 if buffered for later
int insert_packet(reorder_buffer_t *buffer, uint16_t seq, uint8_t *data, size_t size) {
    // First packet sets the expected sequence
    if (!buffer->initialized) {
        buffer->expected_seq = seq;
        buffer->initialized = 1;
    }

        // Out of order - store in buffer
    // Calculate buffer index based on how far ahead this packet is
    int16_t offset = (int16_t)(seq - buffer->expected_seq);
    // Is this the packet we're waiting for?
    if (offset < 0) {
        // Old packet (duplicate or very late) - ignore
        printf("Ignoring old packet: seq=%u (expected=%u)\n", seq, buffer->expected_seq);
        return 0;
    }
    else if (offset >= REORDER_BUFFER_SIZE) {
        // Too far ahead - buffer full or packet is beyond the buffer's window
        printf("Warning: Packet too far ahead, buffer full moving reorder buffer (seq=%u, expected=%u)\n",
                seq, buffer->expected_seq);
        
        return 0;
    }
        
    // Store in buffer
    int slot_index = offset;

    // Check for duplicate packet
    if (buffer->slots[slot_index].valid) {
        return 0;
    }

    buffer->slots[slot_index].seq = seq;
    // Copy payload data into the pre-allocated buffer for this slot
    memcpy(buffer->slots[slot_index].data, data, size);
    buffer->slots[slot_index].size = size;
    buffer->slots[slot_index].valid = 1;

    if (offset > 0) {
        printf("Buffered out-of-order packet: seq=%u at slot %d (expected=%u)\n",
            seq, slot_index, buffer->expected_seq);
        return 1;
    }

    return 0;  // Don't process yet
}

uint8_t* shift_seq(reorder_buffer_t *buffer) {
    buffer->expected_seq++;

    uint8_t* data_to_return = buffer->slots[0].data; 
    // 3. Shift the buffer slots down by one
    for (int i = 0; i < REORDER_BUFFER_SIZE - 1; i++) {
        // Copy the entire struct (metadata and the data pointer) from i+1 to i
        buffer->slots[i] = buffer->slots[i + 1];
    }

    buffer->slots[REORDER_BUFFER_SIZE - 1].data = data_to_return;
    buffer->slots[REORDER_BUFFER_SIZE - 1].valid = 0;
    buffer->slots[REORDER_BUFFER_SIZE - 1].seq = 0;
    buffer->slots[REORDER_BUFFER_SIZE - 1].size = 0;
    get_monotonic_time(&buffer->packet_wait_time);
    return data_to_return;
}

// Check if next expected packet is in buffer (slot 0)
// Returns: pointer to the payload data if found, NULL if not
uint8_t* get_next_packet(reorder_buffer_t *buffer, size_t *size, stats_t *stats) {
    // Check if slot 0 (which corresponds to expected_seq) has the next packet

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

    return NULL; // Next expected packet not in slot 0
}
