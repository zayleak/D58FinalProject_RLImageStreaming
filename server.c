#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "rtp.h"
#include "time_utils.h"

#define CHUNK_SIZE 1400
#define MAX_STORED_PACKETS 1000  
#define WAIT_NACK_MS 5000 // amount of time waiting for final nacks
#define GAP_WAIT_NACK_MS 2000 // amount of time waiting between final retransmission nack requests

typedef struct {
    rtp_packet_t packet;
    size_t size;
    uint16_t seq;
    int valid;
} stored_packet_t;

stored_packet_t packet_storage[MAX_STORED_PACKETS];

void store_packet(rtp_packet_t *packet, size_t size, uint16_t seq) {
    int index = seq % MAX_STORED_PACKETS;
    memcpy(&packet_storage[index].packet, packet, size);
    packet_storage[index].size = size;
    packet_storage[index].seq = seq;
    packet_storage[index].valid = 1;
}

stored_packet_t* get_stored_packet(uint16_t seq) {
    int index = seq % MAX_STORED_PACKETS;
    if (packet_storage[index].valid && packet_storage[index].seq == seq) {
        return &packet_storage[index];
    }
    return NULL;
}

uint8_t* read_image_file(const char *filename, size_t *file_size) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Failed to open image file");
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    *file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *buffer = (uint8_t*)malloc(*file_size);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    
    fread(buffer, 1, *file_size, fp);
    fclose(fp);
    return buffer;
}

uint32_t get_timestamp_ms() {
    struct timeval tv;
    get_monotonic_time(&tv);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_ip> <port> <image_file>\n", argv[0]);
        return 1;
    }
    
    const char *client_ip = argv[1];
    int port = atoi(argv[2]);
    const char *image_file = argv[3];
    
    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;  
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = inet_addr(client_ip);
    
    size_t image_size;
    uint8_t *image_data = read_image_file(image_file, &image_size);
    if (!image_data) {
        close(sockfd);
        return 1;
    }
    
    printf("Enhanced RTP Server with Retransmission\n");
    printf("Image: %s (%zu bytes)\n", image_file, image_size);
    printf("Sending to %s:%d\n\n", client_ip, port);
    
    memset(packet_storage, 0, sizeof(packet_storage));
    
    
    uint32_t ssrc = 0x12345678;
    uint16_t sequence = 0;

    while (1) {
        printf("Sending image...\n");
        size_t offset = 0;
        int packets_sent = 0;
        int retransmissions = 0;
      

        uint32_t timestamp = get_timestamp_ms();

        while (offset < image_size) {
            rtp_packet_t packet;
            size_t chunk_size = (image_size - offset > CHUNK_SIZE) ? 
                                CHUNK_SIZE : (image_size - offset);
        
            int packet_size = create_rtp_packet(&packet, sequence, timestamp,
                                               ssrc, image_data + offset, chunk_size);
        
            // Mark last packet
            if (offset + chunk_size >= image_size) {
                packet.header.marker = 1;
                printf("Packet %d (seq=%u): %zu bytes [LAST PACKET]\n", 
                       packets_sent, sequence, chunk_size);
            } else if (packets_sent % 10 == 0) {
                printf("Packet %d (seq=%u): %zu bytes\n", 
                       packets_sent, sequence, chunk_size);
            }
        
            // Send packet
            sendto(sockfd, &packet, packet_size, 0,
                   (struct sockaddr*)&client_addr, sizeof(client_addr));
        
            store_packet(&packet, packet_size, sequence);
        
            offset += chunk_size;
            sequence++;
            packets_sent++;
        
            usleep(WAIT_NACK_MS); 
        
            nack_packet_t nack;
            struct sockaddr_in nack_addr;
            socklen_t nack_addr_len = sizeof(nack_addr);
        
            ssize_t nack_len = recvfrom(sockfd, &nack, sizeof(nack), 0,
                                        (struct sockaddr*)&nack_addr, &nack_addr_len);
        
            if (nack_len > 0 && nack.type == PACKET_TYPE_NACK) {
                uint16_t missing_seq = ntohs(nack.seq_start);
                printf("\nReceived NACK for seq=%u, retransmitting...\n", missing_seq);
            
                stored_packet_t *stored = get_stored_packet(missing_seq);
                if (stored) {
                    sendto(sockfd, &stored->packet, stored->size, 0,
                           (struct sockaddr*)&client_addr, sizeof(client_addr));
                    retransmissions++;
                    printf("Retransmitted packet seq=%u\n\n", missing_seq);
                } else {
                    printf("Warning: Requested packet seq=%u not in storage\n\n", missing_seq);
                }
            }
        }
    
        printf("\nWaiting for retransmission requests...\n");
        usleep(WAIT_NACK_MS);
    
        for (int i = 0; i < 10; i++) {
            nack_packet_t nack;
            struct sockaddr_in nack_addr;
            socklen_t nack_addr_len = sizeof(nack_addr);
        
            ssize_t nack_len = recvfrom(sockfd, &nack, sizeof(nack), 0,
                                        (struct sockaddr*)&nack_addr, &nack_addr_len);
        
            if (nack_len > 0 && nack.type == PACKET_TYPE_NACK) {
                uint16_t missing_seq = ntohs(nack.seq_start);
                printf("Final NACK for seq=%u\n", missing_seq);
            
                stored_packet_t *stored = get_stored_packet(missing_seq);
                if (stored) {
                    sendto(sockfd, &stored->packet, stored->size, 0,
                           (struct sockaddr*)&client_addr, sizeof(client_addr));
                    retransmissions++;
                }
            }
        
            usleep(GAP_WAIT_NACK_MS); 
        }
        printf("\n=== Transmission Complete ===\n");
        printf("Packets sent: %d\n", packets_sent);
        printf("Retransmissions: %d\n", retransmissions);
    }
    
    free(image_data);
    close(sockfd);
    return 0;
}