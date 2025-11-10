
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "rtp.h"
#include "stats.h"

#define BUFFER_SIZE 2000000  // 2MB buffer 
#define TIMEOUT_SEC 5

// Save frame to file
void save_frame(uint8_t *buffer, size_t size, int frame_num) {
    char filename[64];
    snprintf(filename, sizeof(filename), "received_frame_%04d.jpg", frame_num);
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        printf("Saved to %s\n", filename);
    } else {
        perror("Failed to save frame");
    }
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
    printf("Press Ctrl+C to stop and save the last frame\n\n");
    
    // Allocate buffers
    uint8_t *frame_buffer = (uint8_t*)malloc(BUFFER_SIZE);
    uint8_t *last_complete_frame = (uint8_t*)malloc(BUFFER_SIZE);
    if (!frame_buffer || !last_complete_frame) {
        perror("Buffer allocation failed");
        close(sockfd);
        return 1;
    }
    
    stats_t stats;
    init_stats(&stats);
    
    rtp_packet_t packet;
    size_t frame_offset = 0;
    size_t last_frame_size = 0;
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
                printf("\nTimeout waiting for packets. Saving last received frame...\n");
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
            
            // Copy to last_complete_frame buffer (overwrite previous)
            memcpy(last_complete_frame, frame_buffer, frame_offset);
            last_frame_size = frame_offset;
            
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
            // Frame is complete, copy it as the last complete frame
            memcpy(last_complete_frame, frame_buffer, frame_offset);
            last_frame_size = frame_offset;
        }
        
        // Print stats every 100 packets
        if (stats.packets_received % 100 == 0) {
            print_stats(&stats);
        }
    }
    
    // Save the last complete frame when exiting
    if (last_frame_size > 0) {
        printf("\nSaving last received frame...\n");
        save_frame(last_complete_frame, last_frame_size, 0);
    } else {
        printf("\nNo complete frame received.\n");
    }
    
    print_stats(&stats);
    
    free(frame_buffer);
    free(last_complete_frame);
    close(sockfd);
    return 0;
}