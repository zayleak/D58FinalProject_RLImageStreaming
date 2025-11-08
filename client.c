#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "rtp.h"

#define BUFFER_SIZE 2000000  // 2MB buffer for reconstructing frames
#define TIMEOUT_SEC 5

// Statistics structure
typedef struct {
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t frames_received;
    uint16_t last_seq;
    uint32_t total_bytes;
    struct timeval start_time;
} stats_t;

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
    printf("Elapsed time: %.2f seconds\n", elapsed);
    if (elapsed > 0) {
        printf("Average bitrate: %.2f kbps\n", 
               (stats->total_bytes * 8.0) / (elapsed * 1000.0));
    }
    printf("==================\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    
    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }
    
    printf("RTP Client listening on port %d...\n", port);
    
    // Allocate buffers
    uint8_t *frame_buffer = (uint8_t*)malloc(BUFFER_SIZE);
    if (!frame_buffer) {
        perror("Buffer allocation failed");
        close(sockfd);
        return 1;
    }
    
    stats_t stats;
    init_stats(&stats);
    
    rtp_packet_t packet;
    size_t frame_offset = 0;
    uint32_t current_timestamp = 0;
    int frame_count = 0;
    
    // Receive packets
    while (1) {
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0,
                                    (struct sockaddr*)&sender_addr, &addr_len);
        
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout waiting for packets. Connection may be lost.\n");
                print_stats(&stats);
                break;
            }
            perror("recvfrom failed");
            continue;
        }
        
        // Extract RTP header information
        uint16_t seq = ntohs(packet.header.sequence);
        uint32_t timestamp = ntohl(packet.header.timestamp);
        size_t payload_size = recv_len - sizeof(rtp_header_t);
        
        // Update statistics
        update_stats(&stats, seq, recv_len);
        
        // Check if this is a new frame
        if (current_timestamp != 0 && timestamp != current_timestamp) {
            printf("Frame %d complete: %zu bytes\n", frame_count, frame_offset);
            
            // Save frame to file
            char filename[64];
            snprintf(filename, sizeof(filename), "received_frame_%04d.jpg", frame_count);
            FILE *fp = fopen(filename, "wb");
            if (fp) {
                fwrite(frame_buffer, 1, frame_offset, fp);
                fclose(fp);
                printf("Saved to %s\n", filename);
            }
            
            stats.frames_received++;
            frame_count++;
            frame_offset = 0;
        }
        
        current_timestamp = timestamp;
        
        // Append payload to frame buffer
        if (frame_offset + payload_size < BUFFER_SIZE) {
            memcpy(frame_buffer + frame_offset, packet.payload, payload_size);
            frame_offset += payload_size;
        } else {
            printf("Warning: Frame buffer overflow!\n");
        }
        
        // Check if this is the last packet of the frame (marker bit)
        if (packet.header.marker) {
            printf("Received last packet of frame (marker bit set)\n");
        }
        
        // Print stats every 100 packets
        if (stats.packets_received % 100 == 0) {
            print_stats(&stats);
        }
    }
    
    print_stats(&stats);
    free(frame_buffer);
    close(sockfd);
    return 0;
}