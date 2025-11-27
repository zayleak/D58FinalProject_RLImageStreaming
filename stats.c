
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "stats.h"

// Statistics structure


void init_stats(stats_t *stats) {
    memset(stats, 0, sizeof(stats_t));
    gettimeofday(&stats->start_time, NULL);
}

void update_stats(stats_t *stats, uint16_t seq, size_t bytes) {
    uint16_t expected_seq = stats->last_seq + 1;
    
    if (stats->packets_received > 0 && seq != expected_seq) {
        uint16_t lost = seq - expected_seq;
        stats->packets_lost += lost;
        printf("Warning: Packet loss detected! Expected seq %u, got %u (lost %u)\n",
               expected_seq, seq, lost);
    }
    
    stats->packets_received++;
    stats->total_bytes += bytes;
    stats->last_seq = seq;
}

void print_stats(stats_t *stats) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    double elapsed = (now.tv_sec - stats->start_time.tv_sec) + 
                     (now.tv_usec - stats->start_time.tv_usec) / 1000000.0;
    printf("\n=== Statistics ===\n");
    printf("Packets received: %u\n", stats->packets_received);
    printf("Packets lost: %u\n", stats->packets_lost);
    printf("Frames received: %u\n", stats->frames_received);
    printf("Total bytes: %u\n", stats->total_bytes);
    printf("Retransmit requests: %u\n", stats->retransmit_requests);
    printf("Elapsed time: %.2f seconds\n", elapsed);
    if (elapsed > 0) {
        printf("Average bitrate: %.2f kbps\n", 
               (stats->total_bytes * 8.0) / (elapsed * 1000.0));
        printf("Average frame rate: %.2f fps\n",
               stats->frames_received / elapsed);
    }
    printf("==================\n");
}