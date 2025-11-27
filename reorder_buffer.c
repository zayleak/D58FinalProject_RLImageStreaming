#include "reorder_buffer.h"
#include <stdio.h> // For printf (logging/warnings)

// Initialize reorder buffer and allocate memory for slots
void init_reorder_buffer(reorder_buffer_t *buffer) {
    memset(buffer, 0, sizeof(reorder_buffer_t));
    buffer->initialized = 0;

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

    // Is this the packet we're waiting for?
    if (seq == buffer->expected_seq) {
        // Perfect! This is in order

        buffer->expected_seq++;
        return 1;  // Process immediately
    }

    // Out of order - store in buffer
    // Calculate buffer index based on how far ahead this packet is
    int16_t offset = (int16_t)(seq - buffer->expected_seq);

    if (offset < 0) {
        // Old packet (duplicate or very late) - ignore
        printf("Ignoring old packet: seq=%u (expected=%u)\n", seq, buffer->expected_seq);
        return 0;
    }

    if (offset >= REORDER_BUFFER_SIZE) {
        // Too far ahead - buffer full or packet is beyond the buffer's window
        printf("Warning: Packet too far ahead, buffer full (seq=%u, expected=%u)\n",
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

    printf("Buffered out-of-order packet: seq=%u at slot %d (expected=%u)\n",
        seq, slot_index, buffer->expected_seq);

    return 0;  // Don't process yet
}

// Check if next expected packet is in buffer (slot 0)
// Returns: pointer to the payload data if found, NULL if not
uint8_t* get_next_packet(reorder_buffer_t *buffer, size_t *size) {
    // Check if slot 0 (which corresponds to expected_seq) has the next packet
    if (buffer->slots[0].valid && buffer->slots[0].seq == buffer->expected_seq) {
        // Packet found!

        // 1. Capture the data to be returned (before shifting)
        *size = buffer->slots[0].size;
        // Store the pointer to the data buffer that is about to be released/recycled
        uint8_t* data_to_return = buffer->slots[0].data; 

        // 2. Update expected sequence number
        buffer->expected_seq++;

        // 3. Shift the buffer slots down by one
        for (int i = 0; i < REORDER_BUFFER_SIZE - 1; i++) {
            // Copy the entire struct (metadata and the data pointer) from i+1 to i
            buffer->slots[i] = buffer->slots[i + 1];
        }

        // 4. Invalidate and clear the last slot (which now holds duplicate/garbage metadata)
        // CRITICAL: The data pointer in the last slot is now owned by the slot before it.
        // We ensure we reuse the *original* memory block from the first released slot (data_to_return)
        // by making the last slot point to it. This keeps the memory pool fixed.
        buffer->slots[REORDER_BUFFER_SIZE - 1].data = data_to_return;
        buffer->slots[REORDER_BUFFER_SIZE - 1].valid = 0;
        buffer->slots[REORDER_BUFFER_SIZE - 1].seq = 0;
        buffer->slots[REORDER_BUFFER_SIZE - 1].size = 0;

        // 5. Return the pointer to the payload data of the released packet
        return data_to_return;
    }

    return NULL; // Next expected packet not in slot 0
}

// Check for missing packets (gaps in sequence)
// Returns: sequence number of first missing packet, or 0 if none
uint16_t find_missing_packet(reorder_buffer_t *buffer) {
    if (!buffer->initialized) {
        return 0;
    }

    // If slot 0 is not valid, we are missing the expected sequence number.
    if (!buffer->slots[0].valid) {
        // Iterate to see if any packet exists in the buffer (meaning we have a gap)
        for (int i = 1; i < REORDER_BUFFER_SIZE; i++) {
             if (buffer->slots[i].valid) {
                 // Found a packet (i > 0) while slot 0 is missing.
                 // This confirms a gap starting at buffer->expected_seq.
                 return buffer->expected_seq;
             }
        }
    }
    
    // If slot 0 is valid, get_next_packet handles it. 
    // If the buffer is empty (no valid packets), there is no evidence of a loss, so return 0.

    return 0;
}