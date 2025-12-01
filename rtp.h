#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct {
    uint8_t version:2;     
    uint8_t padding:1;     
    uint8_t extension:1;   
    uint8_t csrc_count:4;   
    uint8_t marker:1;       
    uint8_t payload_type:7; 
    uint16_t sequence;      
    uint32_t timestamp;     
    uint32_t ssrc;          
} __attribute__((packed)) rtp_header_t;

typedef struct {
    rtp_header_t header;
    uint8_t payload[65507];  
} rtp_packet_t;

typedef struct {
    uint8_t type;           
    uint16_t seq_start;     
    uint16_t seq_count;     
} __attribute__((packed)) nack_packet_t;

#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE_JPEG 26
#define MAX_PACKET_SIZE 65535
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - sizeof(rtp_header_t))
#define DEFAULT_PORT 5004

#define PACKET_TYPE_RTP 0
#define PACKET_TYPE_NACK 1

void init_rtp_header(rtp_header_t *header, uint16_t seq, uint32_t timestamp, uint32_t ssrc);
int create_rtp_packet(rtp_packet_t *packet, uint16_t seq, uint32_t timestamp, 
                      uint32_t ssrc, uint8_t *data, size_t data_len);
void print_rtp_header(rtp_header_t *header);
void send_nack(int sockfd, struct sockaddr_in *server_addr, uint16_t seq);

#endif // RTP_H