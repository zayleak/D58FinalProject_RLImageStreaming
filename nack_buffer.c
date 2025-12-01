#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include "nack_buffer.h"
#include "time_utils.h"
#include "rtp.h"


void init_nack_buffer(nack_buffer_t *nb) {

    memset(nb->entries, 0, sizeof(nb->entries));
}

static size_t get_index(uint16_t seq) {
    return seq % NACK_BUFFER_SIZE;
}


static nack_entry_t* get_entry(nack_buffer_t *nb, uint16_t seq) {
    size_t index = get_index(seq);
    nack_entry_t *entry = &nb->entries[index];
    if (entry->seq == seq && entry->retry_count > 0) {
        return entry;
    }

    return entry;
}

static long calculate_min_wait_ms(uint8_t retry_count) {
    if (retry_count == 0) {
        return 0; 
    }
    
    return (long)pow((double)RTT_MS, (double)retry_count);
}

int can_send_nack(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);
    
    if (entry->seq != seq || entry->retry_count == 0) {
  
        return 1;
    }

    if (entry->retry_count >= NACK_MAX_RETRIES) {
        return 0;
    }

    struct timeval now;
    get_monotonic_time(&now);

    long required_wait_ms = calculate_min_wait_ms(entry->retry_count);
    long time_since_last_nack_ms = time_diff_ms(&entry->last_nack_time, &now);
    
    if (time_since_last_nack_ms >= required_wait_ms) {
        printf("Time passed (seq=%u, count=%u). Sent: %ldms, Required: %ldms. Sending NACK.\n", 
                 seq, entry->retry_count, time_since_last_nack_ms, required_wait_ms);
        return 1;
    } else {
        // printf("NACK too soon (seq=%u, count=%u). Sent: %ldms, Required: %ldms. Skipping NACK.\n", 
        //         seq, entry->retry_count, time_since_last_nack_ms, required_wait_ms);
        return 0;
    }
}

void record_nack_attempt(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);


    if (entry->seq != seq || entry->retry_count == 0) {
        entry->seq = seq;
        entry->retry_count = 1;
    } else {
     
        entry->retry_count++;
    }

    get_monotonic_time(&entry->last_nack_time);
}

void clear_nack_entry(nack_buffer_t *nb, uint16_t seq) {
    nack_entry_t *entry = get_entry(nb, seq);

    if (entry->seq == seq) {
        entry->seq = 0;
        entry->retry_count = 0;
        entry->last_nack_time.tv_sec = 0;
        entry->last_nack_time.tv_usec = 0;
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
            printf("NACK Timeout for seq=%u. Retrying (%d/%d)...\n", entry->seq, entry->retry_count + 1, NACK_MAX_RETRIES);
            send_nack(sockfd, server_addr, entry->seq);
            entry->retry_count++;
            entry->last_nack_time = now;
        }
    }
}