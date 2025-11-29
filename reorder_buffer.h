#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define REORDER_BUFFER_SIZE 101  // Can hold up to 100 out-of-order packets
//Note Slot 0 is where expected_seq = current_seq

// Slot in reorder buffer
typedef struct {
    uint16_t seq;           // Sequence number
    uint8_t *data;          // Packet payload (dynamically allocated)
    size_t size;            // Payload size
    int valid;              // 1 = slot contains valid data, 0 = empty
} packet_slot_t;

// Reorder buffer structure
typedef struct {
    packet_slot_t slots[REORDER_BUFFER_SIZE];
    uint16_t expected_seq;  // Next sequence number we expect
    int initialized;        // Has expected_seq been set?
} reorder_buffer_t;

// Function Prototypes

// Initialize reorder buffer and allocate memory for slots
void init_reorder_buffer(reorder_buffer_t *buffer);

// Free reorder buffer (freeing allocated memory for payloads)
void free_reorder_buffer(reorder_buffer_t *buffer);

// Insert packet into reorder buffer
// Returns: 1 if this is the expected packet (in order), 0 if buffered for later
int insert_packet(reorder_buffer_t *buffer, uint16_t seq, uint8_t *data, size_t size);

// Check if next expected packet is in buffer (slot 0)
// Returns: pointer to the payload data if found, NULL if not
uint8_t* get_next_packet(reorder_buffer_t *buffer, size_t *size);

// Check for missing packets (gaps in sequence)
// Returns: sequence number of first missing packet, or 0 if none
uint16_t find_missing_packet(reorder_buffer_t *buffer);

#endif // REORDER_BUFFER_H