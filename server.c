#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include "rtp.h"

#define FRAME_RATE 30  // Frames per second
#define CHUNK_SIZE 1400  // Max payload per packet (safe for MTU)

// Read image file into buffer
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

// Get current timestamp in milliseconds
uint32_t get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
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
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = inet_addr(client_ip);
    
    // Read image file
    size_t image_size;
    uint8_t *image_data = read_image_file(image_file, &image_size);
    if (!image_data) {
        close(sockfd);
        return 1;
    }
    
    printf("Starting RTP Server...\n");
    printf("Image size: %zu bytes\n", image_size);
    printf("Streaming to %s:%d\n", client_ip, port);
    
    // RTP streaming parameters
    uint16_t sequence = 0;
    uint32_t ssrc = 0x12345678;  // Random SSRC identifier
    uint32_t frame_interval_us = 1000000 / FRAME_RATE;  // Microseconds between frames
    
    // Stream continuously (press Ctrl+C to stop)
    while (1) {
        uint32_t frame_timestamp = get_timestamp_ms();
        size_t offset = 0;
        int packet_count = 0;
        
        printf("\nSending frame (seq starts at %u)...\n", sequence);
        
        // Fragment image into multiple RTP packets
        while (offset < image_size) {
            rtp_packet_t packet;
            size_t chunk_size = (image_size - offset > CHUNK_SIZE) ? 
                                CHUNK_SIZE : (image_size - offset);
            

            int packet_size = create_rtp_packet(&packet, sequence, frame_timestamp, 
                                                ssrc, image_data + offset, chunk_size);
            
            // Mark last packet of frame
            if (offset + chunk_size >= image_size) {
                packet.header.marker = 1;
            }
            
            // Send packet
            ssize_t sent = sendto(sockfd, &packet, packet_size, 0,
                                 (struct sockaddr*)&client_addr, sizeof(client_addr));
            
            if (sent < 0) {
                perror("sendto failed");
            } else {
                packet_count++;
            }
            
            offset += chunk_size;
            sequence++;
            
            // Small delay between packets to avoid overwhelming the network
            usleep(100);
        }
        
        printf("Sent %d packets for frame\n", packet_count);
        
        // Wait for next frame interval
        usleep(frame_interval_us);
    }
    
    free(image_data);
    close(sockfd);
    return 0;
}