/**
 * protocol.h
 * 
 * ICMP Tunneling Protocol
 * 
 * Packet Format:
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | Type   | Flags  | Frag ID| Frag # | Total  |   Payload (encrypted)  |
 * | (1)    | (1)    | (2)    | (2)    | (2)    |      (up to 1432)      |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * 
 * Encrypted Payload contains:
 * +--------+--------+--------+--------+--------+--------+--------+
 * | IV (12 bytes)   | Encrypted Data (variable) | Auth Tag (16) |
 * +--------+--------+--------+--------+--------+--------+--------+
 * 
 * Features:
 * - AES-256-GCM encryption
 * - Fragmentation for large packets
 * - Session-based AAD (Additional Authenticated Data)
 * - Packet types: DATA, HANDSHAKE, ACK, KEEPALIVE
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "../crypto/aes_gcm.h"

// Protocol constants
#define PROTO_VERSION 1
#define PROTO_HEADER_SIZE 8              // Type + Flags + FragID + Frag# + Total
#define PROTO_MAX_PAYLOAD 1432           // 1472 ICMP MTU - 40 bytes overhead
#define PROTO_MAX_DATA_SIZE 1372         // Payload - IV(12) - Tag(16) - Header(8) - Reserve(24)
#define PROTO_MAX_FRAGMENTS 256          // Maximum fragments per packet

// Packet types
#define PROTO_TYPE_DATA 0x01             // IP packet data
#define PROTO_TYPE_HANDSHAKE 0x02        // DH key exchange
#define PROTO_TYPE_ACK 0x03              // Acknowledgment
#define PROTO_TYPE_KEEPALIVE 0x04        // Keep connection alive
#define PROTO_TYPE_RESET 0x05            // Reset connection

// Packet flags
#define PROTO_FLAG_ENCRYPTED 0x01        // Payload is encrypted
#define PROTO_FLAG_COMPRESSED 0x02       // Payload is compressed
#define PROTO_FLAG_FRAGMENTED 0x04       // Packet is fragmented
#define PROTO_FLAG_LAST_FRAGMENT 0x08    // Last fragment

// Error codes
#define PROTO_SUCCESS 0
#define PROTO_ERROR_INVALID_PARAM -1
#define PROTO_ERROR_BUFFER_TOO_SMALL -2
#define PROTO_ERROR_ENCRYPTION_FAILED -3
#define PROTO_ERROR_DECRYPTION_FAILED -4
#define PROTO_ERROR_FRAGMENT_OVERFLOW -5
#define PROTO_ERROR_INVALID_TYPE -6
#define PROTO_ERROR_INVALID_FRAGMENT -7
#define PROTO_ERROR_INCOMPLETE -8

/**
 * Protocol header structure
 */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct {
    uint8_t type;                        // Packet type
    uint8_t flags;                       // Packet flags
    uint16_t fragment_id;                // Fragment ID (for reassembly)
    uint16_t fragment_num;               // Fragment number (0-based)
    uint16_t total_fragments;            // Total number of fragments
}
#ifndef _MSC_VER
__attribute__((packed))
#endif
proto_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/**
 * Protocol packet structure
 */
typedef struct {
    proto_header_t header;               // Protocol header
    uint8_t payload[PROTO_MAX_PAYLOAD];  // Encrypted payload
    size_t payload_size;                 // Size of payload
} proto_packet_t;

/**
 * Fragment reassembly context
 */
typedef struct {
    uint16_t fragment_id;                // Fragment ID
    uint16_t total_fragments;            // Total expected fragments
    uint16_t received_fragments;         // Fragments received so far
    uint8_t *fragments[PROTO_MAX_FRAGMENTS]; // Fragment buffers
    size_t fragment_sizes[PROTO_MAX_FRAGMENTS]; // Fragment sizes
    time_t start_time;                   // Reassembly start time
    uint8_t active;                      // Context active flag
} proto_reassembly_ctx_t;

/**
 * Encapsulate IP packet into ICMP tunnel packet
 * 
 * @param ip_packet IP packet data to encapsulate
 * @param ip_size Size of IP packet
 * @param session_key AES-256 session key
 * @param session_id Session ID (for AAD)
 * @param sequence Sequence number (for AAD)
 * @param output Output buffer for ICMP packets
 * @param output_count Output: number of packets created
 * @param max_packets Maximum packets to create
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 * 
 * Note: If IP packet > PROTO_MAX_DATA_SIZE, it will be fragmented
 *       Each fragment is encrypted separately with unique IV
 * 
 * Example:
 *   proto_packet_t packets[10];
 *   size_t count;
 *   proto_encapsulate(ip_data, 500, key, 0x1234, 42, packets, &count, 10);
 */
int proto_encapsulate(const uint8_t *ip_packet, size_t ip_size,
                       const uint8_t session_key[32],
                       uint16_t session_id, uint16_t sequence,
                       proto_packet_t *output, size_t *output_count,
                       size_t max_packets);

/**
 * Decapsulate ICMP tunnel packet to IP packet
 * 
 * @param packet ICMP tunnel packet
 * @param session_key AES-256 session key
 * @param session_id Session ID (for AAD verification)
 * @param sequence Sequence number (for AAD verification)
 * @param output Output buffer for IP packet
 * @param output_size Output: size of IP packet
 * @param max_output_size Maximum output buffer size
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 * 
 * Note: Handles single packets and reassembles fragments
 * 
 * Example:
 *   uint8_t ip_data[2048];
 *   size_t ip_size;
 *   proto_decapsulate(&packet, key, 0x1234, 42, ip_data, &ip_size, 2048);
 */
int proto_decapsulate(const proto_packet_t *packet,
                       const uint8_t session_key[32],
                       uint16_t session_id, uint16_t sequence,
                       uint8_t *output, size_t *output_size,
                       size_t max_output_size);

/**
 * Add fragment to reassembly context
 * 
 * @param ctx Reassembly context
 * @param packet Fragmented packet
 * @param session_key Session key for decryption
 * @param session_id Session ID
 * @param sequence Sequence number
 * 
 * @return PROTO_SUCCESS if fragment added, PROTO_ERROR_INCOMPLETE if more needed,
 *         positive value (complete size) when reassembly is complete
 * 
 * Example:
 *   proto_reassembly_ctx_t ctx = {0};
 *   int result = proto_add_fragment(&ctx, &packet, key, sid, seq);
 *   if (result > 0) {
 *       // Reassembly complete, result = total size
 *   }
 */
int proto_add_fragment(proto_reassembly_ctx_t *ctx, const proto_packet_t *packet,
                        const uint8_t session_key[32],
                        uint16_t session_id, uint16_t sequence);

/**
 * Get reassembled packet data
 * 
 * @param ctx Reassembly context
 * @param output Output buffer
 * @param output_size Output: size of data
 * @param max_output_size Maximum output buffer size
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 * 
 * Note: Call after proto_add_fragment returns positive (complete)
 */
int proto_get_reassembled(proto_reassembly_ctx_t *ctx, uint8_t *output,
                           size_t *output_size, size_t max_output_size);

/**
 * Reset reassembly context
 * 
 * @param ctx Reassembly context to reset
 */
void proto_reset_reassembly(proto_reassembly_ctx_t *ctx);

/**
 * Create handshake packet (DH public key)
 * 
 * @param dh_public_key DH public key (256 bytes)
 * @param output Output packet
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 */
int proto_create_handshake(const uint8_t dh_public_key[256], proto_packet_t *output);

/**
 * Parse handshake packet
 * 
 * @param packet Handshake packet
 * @param dh_public_key Output: DH public key (256 bytes)
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 */
int proto_parse_handshake(const proto_packet_t *packet, uint8_t dh_public_key[256]);

/**
 * Create keepalive packet
 * 
 * @param output Output packet
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 */
int proto_create_keepalive(proto_packet_t *output);

/**
 * Create ACK packet
 * 
 * @param ack_sequence Sequence number to acknowledge
 * @param output Output packet
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 */
int proto_create_ack(uint16_t ack_sequence, proto_packet_t *output);

/**
 * Parse ACK packet
 * 
 * @param packet ACK packet
 * @param ack_sequence Output: acknowledged sequence number
 * 
 * @return PROTO_SUCCESS on success, error code otherwise
 */
int proto_parse_ack(const proto_packet_t *packet, uint16_t *ack_sequence);

/**
 * Check if packet is fragmented
 * 
 * @param packet Packet to check
 * @return 1 if fragmented, 0 otherwise
 */
int proto_is_fragmented(const proto_packet_t *packet);

/**
 * Get packet type string
 * 
 * @param type Packet type
 * @return Human-readable type string
 */
const char* proto_type_string(uint8_t type);

/**
 * Get error message string
 * 
 * @param error_code Error code
 * @return Human-readable error message
 */
const char* proto_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
