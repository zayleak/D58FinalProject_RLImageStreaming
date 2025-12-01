#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h> 
#include "rtp.h"
#include "stats.h"
#include "reorder_buffer.h"
#include "jitter_buffer.h"
#include "nack_buffer.h" 
#include "time_utils.h"  

#define BUFFER_SIZE 10000000 
#define TIMEOUT_SEC 5
#define CHUNK_SIZE 1400


int is_valid_jpeg(uint8_t *buf, size_t size) {
    if (size < 4) return 0;

    if (buf[0] != 0xFF || buf[1] != 0xD8) return 0;
    if (buf[size-2] != 0xFF || buf[size-1] != 0xD9) return 0;

    return 1;
}

void save_frame(uint8_t *buffer, size_t size, int frame_num) {
    if (!is_valid_jpeg(buffer, size)) {
        return;
    }
    char filename[64];
    snprintf(filename, sizeof(filename), "frames/received_frame_%04d.jpg", frame_num);
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        printf("Saved frame %d to %s\n", frame_num, filename);
    } else {
        perror("Failed to save frame");
    }
}


void process_packet(uint8_t *frame_buffer, size_t *frame_offset, 
                    uint16_t seq, uint8_t *payload, size_t payload_size,
                    uint16_t frame_start_seq) {
    size_t position = (seq - frame_start_seq) * CHUNK_SIZE; 
    
    if (position + payload_size < BUFFER_SIZE) {
        memcpy(frame_buffer + position, payload, payload_size);
        if (position + payload_size > *frame_offset) {
            *frame_offset = position + payload_size;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }
    
    printf("RTP Client listening on port %d...\n", port);
    printf("Press Ctrl+C to stop and save the last frame\n\n");
    
    reorder_buffer_t reorder_buf;
    jitter_buffer_t jitter_buf;
    nack_buffer_t nack_buf; 
    
    init_reorder_buffer(&reorder_buf);
    init_jitter_buffer(&jitter_buf);
    init_nack_buffer(&nack_buf); 
    
    uint8_t *frame_buffer = (uint8_t*)malloc(BUFFER_SIZE);
    uint8_t *last_complete_frame = (uint8_t*)malloc(BUFFER_SIZE);
    if (!frame_buffer || !last_complete_frame) {
        perror("Buffer allocation failed");
        close(sockfd);
        return 1;
    }
    
    stats_t stats;
    init_stats(&stats);
    
    size_t frame_offset = 0;
    size_t last_frame_size = 0;
    uint32_t current_timestamp = 0;
    int frame_count = 0;
    uint16_t frame_start_seq = 0;
    uint16_t frame_end_seq = 0; 
    uint16_t max_seq_received = 0;
    int first_packet = 1;

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);

    while (1) {
        rtp_packet_t packet;
        ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0,
                                    (struct sockaddr*)&server_addr, &server_addr_len);
        
        struct timeval now;
        get_monotonic_time(&now); 
        
        if (recv_len > 0) {
            stats.packets_received++;
            stats.total_bytes += recv_len;
            
            uint16_t seq = ntohs(packet.header.sequence);

            clear_nack_entry(&nack_buf, seq);

            if (first_packet) {
                max_seq_received = seq;
                first_packet = 0;
            } else {
                int16_t diff = seq - max_seq_received;
            
                if (diff > 1 && diff < 100) { 
                    printf("Gap detected! Last: %u, Current: %u. Checking %d packets for NACK.\n", 
                            max_seq_received, seq, diff - 1);
                
                    for (int i = 1; i < diff; i++) {
                        uint16_t missing_seq = max_seq_received + i;
                        
                        send_nack(sockfd, &server_addr, missing_seq);
                        record_nack_attempt(&nack_buf, missing_seq);
                        stats.retransmit_requests++;
                    }
                }
                if (diff > 0) max_seq_received = seq;
            }

            jitter_buffer_add(&jitter_buf, &packet, recv_len);
        }


        manage_nack_timeouts(&nack_buf, sockfd, &server_addr);
        size_t jitter_packet_size;
        rtp_packet_t *ready_packet = jitter_buffer_get(&jitter_buf, &jitter_packet_size);

        if (ready_packet != NULL) {
            uint16_t seq = ntohs(ready_packet->header.sequence);
            uint32_t timestamp = ntohl(ready_packet->header.timestamp);
            size_t payload_size = jitter_packet_size - sizeof(rtp_header_t);
            
            if (current_timestamp != 0 && timestamp != current_timestamp) {
                printf("--- Frame boundary detected (TS change). Resetting state for Frame %d ---\n", frame_count);
                
                current_timestamp = 0; 
                frame_offset = 0;
                frame_end_seq = 0; 
                memset(frame_buffer, 0, BUFFER_SIZE);
                init_reorder_buffer(&reorder_buf);
                init_nack_buffer(&nack_buf);
            }
            
            if (current_timestamp == 0) {
                current_timestamp = timestamp;
                frame_start_seq = seq;
            }

            if (ready_packet->header.marker) {
                frame_end_seq = seq;
                printf("Received last packet (marker bit set)\n");
            }
            
            int in_order = insert_packet(&reorder_buf, seq, 
                                         ready_packet->payload, payload_size);
            if (!in_order) stats.packets_reordered++;
            
            size_t buffered_size;
            uint8_t *buffered_data = get_next_packet(&reorder_buf, &buffered_size, &stats);
            
            while (buffered_data != NULL) {
                uint16_t buffered_seq = reorder_buf.expected_seq - 1;
                
                process_packet(frame_buffer, &frame_offset, buffered_seq,
                              buffered_data, buffered_size, frame_start_seq);
                
                if (buffered_seq == frame_end_seq && frame_end_seq != 0) {
                    printf("Frame %d complete (Marker Bit): %zu bytes\n", frame_count, frame_offset);
                    save_frame(frame_buffer, frame_offset, frame_count);
                    
                    stats.frames_received++;
                    frame_count++;

                    frame_offset = 0;
                    memset(frame_buffer, 0, BUFFER_SIZE);
                    current_timestamp = 0; 
                    frame_start_seq = 0;
                    frame_end_seq = 0; 
                    init_reorder_buffer(&reorder_buf);
                    init_nack_buffer(&nack_buf);
                    break; 
                }
                
                buffered_data = get_next_packet(&reorder_buf, &buffered_size, &stats);
            }
        }
        
        if (stats.packets_received % 100 == 0 && stats.packets_received > 0) {
            print_stats(&stats);
        }
    }
    
    if (last_frame_size > 0) {
        printf("\nSaving last received frame...\n");
        save_frame(last_complete_frame, last_frame_size, 0);
    }
    
    print_stats(&stats);
    
    free(frame_buffer);
    free(last_complete_frame);
    close(sockfd);
    return 0;
}