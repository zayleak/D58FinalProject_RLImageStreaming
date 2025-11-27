#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

// RTP Header Structure (12 bytes minimum)
typedef struct {
    uint8_t version:2;      // RTP version (should be 2)
    uint8_t padding:1;      // Padding flag
    uint8_t extension:1;    // Extension flag
    uint8_t csrc_count:4;   // CSRC count
    uint8_t marker:1;       // Marker bit
    uint8_t payload_type:7; // Payload type
    uint16_t sequence;      // Sequence number
    uint32_t timestamp;     // Timestamp
    uint32_t ssrc;          // Synchronization source identifier
} __attribute__((packed)) rtp_header_t;

// RTP Packet Structure
typedef struct {
    rtp_header_t header;
    uint8_t payload[65507];  // Max UDP payload size
} rtp_packet_t;

typedef struct {
    uint8_t type;           // 0 = RTP data, 1 = NACK request
    uint16_t seq_start;     // First missing sequence number
    uint16_t seq_count;     // Number of consecutive missing packets
} __attribute__((packed)) nack_packet_t;

// Configuration constants
#define RTP_VERSION 2
#define RTP_PAYLOAD_TYPE_JPEG 26
#define MAX_PACKET_SIZE 65535
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - sizeof(rtp_header_t))
#define DEFAULT_PORT 5004

#define PACKET_TYPE_RTP 0
#define PACKET_TYPE_NACK 1

// Function prototypes
void init_rtp_header(rtp_header_t *header, uint16_t seq, uint32_t timestamp, uint32_t ssrc);
int create_rtp_packet(rtp_packet_t *packet, uint16_t seq, uint32_t timestamp, 
                      uint32_t ssrc, uint8_t *data, size_t data_len);
void print_rtp_header(rtp_header_t *header);

#endif // RTP_H