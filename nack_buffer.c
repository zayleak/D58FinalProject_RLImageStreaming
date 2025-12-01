#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include "nack_buffer.h"
#include "time_utils.h"
#include "rtp.h"


void init_nack_buffer(nack_buffer_t *nb) {
    // Clear all entries. By setting retry_count to 0 and seq to 0, 
    // the entry is marked as unused.
    memset(nb->entries, 0, sizeof(nb->entries));
}

// Helper to get the buffer index based on sequence number
static size_t get_index(uint16_t seq) {
    return seq % NACK_BUFFER_SIZE;
}

// Helper to get the current entry for a sequence number
static nack_entry_t* get_entry(nack_buffer_t *nb, uint16_t seq) {
    size_t index = get_index(seq);
    nack_entry_t *entry = &nb->entries[index];

    // Check if the entry is tracking the desired sequence number.
    // Due to the modulo arithmetic, different sequences can map to the same index.
    // If the sequence numbers match, this is our entry.
    if (entry->seq == seq && entry->retry_count > 0) {
        return entry;
    }
    
    // If the entry is either unused (count=0) or tracking a different sequence,
    // we will return the pointer to the index slot for potential insertion.
    return entry;
}

// Calculates the minimum required wait time (in ms) before the next NACK retry.
// Uses exponential backoff based on retry_count and RTT.
static long calculate_min_wait_ms(uint8_t retry_count) {
    if (retry_count == 0) {
        return 0; 
    }
    
    return (long)pow((double)RTT_MS, (double)retry_count);
}

int can_send_nack(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);
    
    // Case 1: First NACK attempt for this sequence number.
    if (entry->seq != seq || entry->retry_count == 0) {
        // Since this is the first NACK, we should send it immediately.
        return 1;
    }

    // Case 2: Max retries already sent.
    if (entry->retry_count >= NACK_MAX_RETRIES) {
        // printf("NACK limit reached for seq=%u (%u/%d)\n", seq, entry->retry_count, NACK_MAX_RETRIES);
        return 0;
    }

    // Case 3: Check if enough time has passed since the last NACK.
    struct timeval now;
    get_monotonic_time(&now);

    long required_wait_ms = calculate_min_wait_ms(entry->retry_count);
    long time_since_last_nack_ms = time_diff_ms(&entry->last_nack_time, &now);
    
    if (time_since_last_nack_ms >= required_wait_ms) {
        // Enough time has passed.
        printf("Time passed (seq=%u, count=%u). Sent: %ldms, Required: %ldms. Sending NACK.\n", 
                 seq, entry->retry_count, time_since_last_nack_ms, required_wait_ms);
        return 1;
    } else {
        // Not enough time has passed.
        // printf("NACK too soon (seq=%u, count=%u). Sent: %ldms, Required: %ldms. Skipping NACK.\n", 
        //         seq, entry->retry_count, time_since_last_nack_ms, required_wait_ms);
        return 0;
    }
}

void record_nack_attempt(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);

    // If this slot was tracking an old sequence or was empty (seq=0), initialize it.
    if (entry->seq != seq || entry->retry_count == 0) {
        entry->seq = seq;
        entry->retry_count = 1;
    } else {
        // Otherwise, just increment the count.
        entry->retry_count++;
    }

    get_monotonic_time(&entry->last_nack_time);
}

void clear_nack_entry(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);

    // Only clear if the slot actually contains the sequence number we received.
    if (entry->seq == seq) {
        entry->seq = 0;
        entry->retry_count = 0;
        entry->last_nack_time.tv_sec = 0;
        entry->last_nack_time.tv_usec = 0;
        // printf("Cleared NACK entry for received seq=%u\n", seq);
    }
}

void manage_nack_timeouts(nack_buffer_t *nb, int sockfd, struct sockaddr_in *server_addr) {
    struct timeval now;
    get_monotonic_time(&now);

    for (int i = 0; i < NACK_BUFFER_SIZE; i++) {
        nack_entry_t *entry = &nb->entries[i];
        if (entry->retry_count == 0) continue;
        if (entry->retry_count >= NACK_MAX_RETRIES) {
            entry->seq = 0;
            entry->retry_count = 0;
            continue;
        }

        long required_wait_ms = calculate_min_wait_ms(entry->retry_count);
        long elapsed = time_diff_ms(&entry->last_nack_time, &now);

        if (elapsed >= required_wait_ms) {
            // Time to retry!
            printf("NACK Timeout for seq=%u. Retrying (%d/%d)...\n", entry->seq, entry->retry_count + 1, NACK_MAX_RETRIES);
            send_nack(sockfd, server_addr, entry->seq);
            // Update stats in the entry
            entry->retry_count++;
            entry->last_nack_time = now;
        }
    }
}