#ifndef NACK_BUFFER_H
#define NACK_BUFFER_H

#include <stdint.h>
#include <sys/time.h>
#include <string.h>

#define NACK_BUFFER_SIZE 256
#define NACK_MAX_RETRIES 3
#define RTT_MS 20

typedef struct {
    uint16_t seq;           // Sequence number being tracked
    uint8_t retry_count;    // Number of NACKs sent for this seq
    struct timeval last_nack_time; // Monotonic time of the last NACK request
} nack_entry_t;

typedef struct {
    nack_entry_t entries[NACK_BUFFER_SIZE];
} nack_buffer_t;


void init_nack_buffer(nack_buffer_t *nb);
int can_send_nack(nack_buffer_t *nb, uint16_t seq);
void record_nack_attempt(nack_buffer_t *nb, uint16_t seq);
void clear_nack_entry(nack_buffer_t *nb, uint16_t seq);
void manage_nack_timeouts(nack_buffer_t *nb, int sockfd, struct sockaddr_in *server_addr);

#endif // NACK_BUFFER_H