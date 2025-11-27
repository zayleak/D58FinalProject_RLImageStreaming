#include "rtp.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

void init_rtp_header(rtp_header_t *header, uint16_t seq, uint32_t timestamp, uint32_t ssrc) {
    memset(header, 0, sizeof(rtp_header_t));
    header->version = RTP_VERSION;
    header->padding = 0;
    header->extension = 0;
    header->csrc_count = 0;
    header->marker = 0;
    header->payload_type = RTP_PAYLOAD_TYPE_JPEG;
    header->sequence = htons(seq);
    header->timestamp = htonl(timestamp);
    header->ssrc = htonl(ssrc);
}

int create_rtp_packet(rtp_packet_t *packet, uint16_t seq, uint32_t timestamp, 
                      uint32_t ssrc, uint8_t *data, size_t data_len) {
    if (data_len > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Error: Payload size exceeds maximum\n");
        return -1;
    }
    
    init_rtp_header(&packet->header, seq, timestamp, ssrc);
    memcpy(packet->payload, data, data_len);
    
    return sizeof(rtp_header_t) + data_len;
}

// Print RTP header information (for debugging)
void print_rtp_header(rtp_header_t *header) {
    printf("=== RTP Header ===\n");
    printf("Version: %d\n", header->version);
    printf("Payload Type: %d\n", header->payload_type);
    printf("Sequence Number: %u\n", ntohs(header->sequence));
    printf("Timestamp: %u\n", ntohl(header->timestamp));
    printf("SSRC: 0x%08x\n", ntohl(header->ssrc));
    printf("==================\n");
}