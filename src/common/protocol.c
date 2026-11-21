/**
 * protocol.c
 * 
 * Implementation of ICMP Tunneling Protocol
 */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Generate AAD (Additional Authenticated Data) for AES-GCM
 */
static void generate_aad(uint16_t session_id, uint16_t sequence, uint8_t aad[4]) {
    aad[0] = (session_id >> 8) & 0xFF;
    aad[1] = session_id & 0xFF;
    aad[2] = (sequence >> 8) & 0xFF;
    aad[3] = sequence & 0xFF;
}

/**
 * Encapsulate IP packet (with fragmentation if needed)
 */
int proto_encapsulate(const uint8_t *ip_packet, size_t ip_size,
                       const uint8_t session_key[32],
                       uint16_t session_id, uint16_t sequence,
                       proto_packet_t *output, size_t *output_count,
                       size_t max_packets) {
    if (!ip_packet || !session_key || !output || !output_count || ip_size == 0) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    // Generate AAD
    uint8_t aad[4];
    generate_aad(session_id, sequence, aad);
    
    // Calculate number of fragments needed
    size_t fragments_needed = (ip_size + PROTO_MAX_DATA_SIZE - 1) / PROTO_MAX_DATA_SIZE;
    
    if (fragments_needed > max_packets) {
        return PROTO_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (fragments_needed > PROTO_MAX_FRAGMENTS) {
        return PROTO_ERROR_FRAGMENT_OVERFLOW;
    }
    
    // Generate random fragment ID
    uint16_t fragment_id = (uint16_t)(rand() % 0xFFFF);
    
    *output_count = 0;
    size_t offset = 0;
    
    for (size_t i = 0; i < fragments_needed; i++) {
        proto_packet_t *pkt = &output[i];
        
        // Calculate fragment size
        size_t fragment_size = PROTO_MAX_DATA_SIZE;
        if (offset + fragment_size > ip_size) {
            fragment_size = ip_size - offset;
        }
        
        // Fill header
        pkt->header.type = PROTO_TYPE_DATA;
        pkt->header.flags = PROTO_FLAG_ENCRYPTED;
        
        if (fragments_needed > 1) {
            pkt->header.flags |= PROTO_FLAG_FRAGMENTED;
            if (i == fragments_needed - 1) {
                pkt->header.flags |= PROTO_FLAG_LAST_FRAGMENT;
            }
        }
        
        pkt->header.fragment_id = fragment_id;
        pkt->header.fragment_num = (uint16_t)i;
        pkt->header.total_fragments = (uint16_t)fragments_needed;
        
        // Generate random IV
        uint8_t iv[12];
        for (int j = 0; j < 12; j++) {
            iv[j] = (uint8_t)(rand() % 256);
        }
        
        // Store IV at beginning of payload
        memcpy(pkt->payload, iv, 12);
        
        // Encrypt fragment (after IV)
        uint8_t tag[16];
        
        int result = aes_gcm_encrypt(
            session_key, 32,
            iv, 12,
            ip_packet + offset, fragment_size,
            aad, 4,
            pkt->payload + 12, tag
        );
        
        if (result != 0) {
            return PROTO_ERROR_ENCRYPTION_FAILED;
        }
        
        // Append tag after encrypted data
        memcpy(pkt->payload + 12 + fragment_size, tag, 16);
        pkt->payload_size = 12 + fragment_size + 16;
        
        offset += fragment_size;
        (*output_count)++;
    }
    
    return PROTO_SUCCESS;
}

/**
 * Decapsulate single (non-fragmented) packet
 */
int proto_decapsulate(const proto_packet_t *packet,
                       const uint8_t session_key[32],
                       uint16_t session_id, uint16_t sequence,
                       uint8_t *output, size_t *output_size,
                       size_t max_output_size) {
    if (!packet || !session_key || !output || !output_size) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    // Check if encrypted
    if (!(packet->header.flags & PROTO_FLAG_ENCRYPTED)) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    // Check if fragmented (should use proto_add_fragment instead)
    if (packet->header.flags & PROTO_FLAG_FRAGMENTED) {
        return PROTO_ERROR_INVALID_FRAGMENT;
    }
    
    // Generate AAD
    uint8_t aad[4];
    generate_aad(session_id, sequence, aad);
    
    // Extract IV, ciphertext, and tag from payload
    //  Payload format: [IV: 12 bytes][Ciphertext: N bytes][Tag: 16 bytes]
    if (packet->payload_size < 12 + 16) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    const uint8_t *iv = packet->payload;
    size_t ciphertext_size = packet->payload_size - 12 - 16;
    const uint8_t *ciphertext = packet->payload + 12;
    const uint8_t *tag = packet->payload + 12 + ciphertext_size;
    
    // Decrypt
    int result = aes_gcm_decrypt(
        session_key, 32,
        iv, 12,
        ciphertext, ciphertext_size,
        aad, 4,
        tag,
        output
    );
    
    if (result != 0) {
        return PROTO_ERROR_DECRYPTION_FAILED;
    }
    
    *output_size = ciphertext_size;
    
    if (*output_size > max_output_size) {
        return PROTO_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PROTO_SUCCESS;
}

/**
 * Add fragment to reassembly context
 */
int proto_add_fragment(proto_reassembly_ctx_t *ctx, const proto_packet_t *packet,
                        const uint8_t session_key[32],
                        uint16_t session_id, uint16_t sequence) {
    if (!ctx || !packet || !session_key) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    // Check if fragmented
    if (!(packet->header.flags & PROTO_FLAG_FRAGMENTED)) {
        return PROTO_ERROR_INVALID_FRAGMENT;
    }
    
    // Initialize context if first fragment
    if (!ctx->active) {
        ctx->fragment_id = packet->header.fragment_id;
        ctx->total_fragments = packet->header.total_fragments;
        ctx->received_fragments = 0;
        ctx->start_time = time(NULL);
        ctx->active = 1;
        memset(ctx->fragments, 0, sizeof(ctx->fragments));
        memset(ctx->fragment_sizes, 0, sizeof(ctx->fragment_sizes));
    }
    
    // Verify fragment ID matches
    if (ctx->fragment_id != packet->header.fragment_id) {
        return PROTO_ERROR_INVALID_FRAGMENT;
    }
    
    // Check fragment number
    uint16_t frag_num = packet->header.fragment_num;
    if (frag_num >= PROTO_MAX_FRAGMENTS || frag_num >= ctx->total_fragments) {
        return PROTO_ERROR_INVALID_FRAGMENT;
    }
    
    // Check if already received
    if (ctx->fragments[frag_num] != NULL) {
        return PROTO_ERROR_INVALID_FRAGMENT;
    }
    
    // Decrypt fragment
    uint8_t aad[4];
    generate_aad(session_id, sequence, aad);
    
    ctx->fragments[frag_num] = (uint8_t*)malloc(PROTO_MAX_DATA_SIZE);
    if (!ctx->fragments[frag_num]) {
        return PROTO_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Extract IV, ciphertext, and tag
    const uint8_t *iv = packet->payload;
    size_t ciphertext_size = packet->payload_size - 12 - 16;
    const uint8_t *ciphertext = packet->payload + 12;
    const uint8_t *tag = packet->payload + 12 + ciphertext_size;
    
    int result = aes_gcm_decrypt(
        session_key, 32,
        iv, 12,
        ciphertext, ciphertext_size,
        aad, 4,
        tag,
        ctx->fragments[frag_num]
    );
    
    if (result != 0) {
        free(ctx->fragments[frag_num]);
        ctx->fragments[frag_num] = NULL;
        return PROTO_ERROR_DECRYPTION_FAILED;
    }
    
    ctx->fragment_sizes[frag_num] = ciphertext_size;
    ctx->received_fragments++;
    
    // Check if complete
    if (ctx->received_fragments == ctx->total_fragments) {
        // Calculate total size
        size_t total_size = 0;
        for (uint16_t i = 0; i < ctx->total_fragments; i++) {
            total_size += ctx->fragment_sizes[i];
        }
        return (int)total_size;
    }
    
    return PROTO_ERROR_INCOMPLETE;
}

/**
 * Get reassembled packet
 */
int proto_get_reassembled(proto_reassembly_ctx_t *ctx, uint8_t *output,
                           size_t *output_size, size_t max_output_size) {
    if (!ctx || !output || !output_size || !ctx->active) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    // Calculate total size
    size_t total_size = 0;
    for (uint16_t i = 0; i < ctx->total_fragments; i++) {
        total_size += ctx->fragment_sizes[i];
    }
    
    if (total_size > max_output_size) {
        return PROTO_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Copy fragments
    size_t offset = 0;
    for (uint16_t i = 0; i < ctx->total_fragments; i++) {
        if (ctx->fragments[i]) {
            memcpy(output + offset, ctx->fragments[i], ctx->fragment_sizes[i]);
            offset += ctx->fragment_sizes[i];
        }
    }
    
    *output_size = total_size;
    
    return PROTO_SUCCESS;
}

/**
 * Reset reassembly context
 */
void proto_reset_reassembly(proto_reassembly_ctx_t *ctx) {
    if (!ctx) return;
    
    for (int i = 0; i < PROTO_MAX_FRAGMENTS; i++) {
        if (ctx->fragments[i]) {
            free(ctx->fragments[i]);
            ctx->fragments[i] = NULL;
        }
    }
    
    memset(ctx, 0, sizeof(proto_reassembly_ctx_t));
}

/**
 * Create handshake packet
 */
int proto_create_handshake(const uint8_t dh_public_key[256], proto_packet_t *output) {
    if (!dh_public_key || !output) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    output->header.type = PROTO_TYPE_HANDSHAKE;
    output->header.flags = 0;
    output->header.fragment_id = 0;
    output->header.fragment_num = 0;
    output->header.total_fragments = 1;
    
    memcpy(output->payload, dh_public_key, 256);
    output->payload_size = 256;
    
    return PROTO_SUCCESS;
}

/**
 * Parse handshake packet
 */
int proto_parse_handshake(const proto_packet_t *packet, uint8_t dh_public_key[256]) {
    if (!packet || !dh_public_key) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    if (packet->header.type != PROTO_TYPE_HANDSHAKE) {
        return PROTO_ERROR_INVALID_TYPE;
    }
    
    if (packet->payload_size < 256) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    memcpy(dh_public_key, packet->payload, 256);
    
    return PROTO_SUCCESS;
}

/**
 * Create keepalive packet
 */
int proto_create_keepalive(proto_packet_t *output) {
    if (!output) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    output->header.type = PROTO_TYPE_KEEPALIVE;
    output->header.flags = 0;
    output->header.fragment_id = 0;
    output->header.fragment_num = 0;
    output->header.total_fragments = 1;
    output->payload_size = 0;
    
    return PROTO_SUCCESS;
}

/**
 * Create ACK packet
 */
int proto_create_ack(uint16_t ack_sequence, proto_packet_t *output) {
    if (!output) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    output->header.type = PROTO_TYPE_ACK;
    output->header.flags = 0;
    output->header.fragment_id = 0;
    output->header.fragment_num = 0;
    output->header.total_fragments = 1;
    
    output->payload[0] = (ack_sequence >> 8) & 0xFF;
    output->payload[1] = ack_sequence & 0xFF;
    output->payload_size = 2;
    
    return PROTO_SUCCESS;
}

/**
 * Parse ACK packet
 */
int proto_parse_ack(const proto_packet_t *packet, uint16_t *ack_sequence) {
    if (!packet || !ack_sequence) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    if (packet->header.type != PROTO_TYPE_ACK) {
        return PROTO_ERROR_INVALID_TYPE;
    }
    
    if (packet->payload_size < 2) {
        return PROTO_ERROR_INVALID_PARAM;
    }
    
    *ack_sequence = (packet->payload[0] << 8) | packet->payload[1];
    
    return PROTO_SUCCESS;
}

/**
 * Check if packet is fragmented
 */
int proto_is_fragmented(const proto_packet_t *packet) {
    if (!packet) return 0;
    return (packet->header.flags & PROTO_FLAG_FRAGMENTED) ? 1 : 0;
}

/**
 * Get packet type string
 */
const char* proto_type_string(uint8_t type) {
    switch (type) {
        case PROTO_TYPE_DATA:
            return "DATA";
        case PROTO_TYPE_HANDSHAKE:
            return "HANDSHAKE";
        case PROTO_TYPE_ACK:
            return "ACK";
        case PROTO_TYPE_KEEPALIVE:
            return "KEEPALIVE";
        case PROTO_TYPE_RESET:
            return "RESET";
        case PROTO_TYPE_RESOURCE_REQUEST:
            return "RESOURCE_REQUEST";
        case PROTO_TYPE_RESOURCE_RESPONSE:
            return "RESOURCE_RESPONSE";
        default:
            return "UNKNOWN";
    }
}

/**
 * Get error message string
 */
const char* proto_error_string(int error_code) {
    switch (error_code) {
        case PROTO_SUCCESS:
            return "Success";
        case PROTO_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case PROTO_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case PROTO_ERROR_ENCRYPTION_FAILED:
            return "Encryption failed";
        case PROTO_ERROR_DECRYPTION_FAILED:
            return "Decryption failed";
        case PROTO_ERROR_FRAGMENT_OVERFLOW:
            return "Too many fragments";
        case PROTO_ERROR_INVALID_TYPE:
            return "Invalid packet type";
        case PROTO_ERROR_INVALID_FRAGMENT:
            return "Invalid fragment";
        case PROTO_ERROR_INCOMPLETE:
            return "Incomplete (waiting for more fragments)";
        default:
            return "Unknown error";
    }
}
