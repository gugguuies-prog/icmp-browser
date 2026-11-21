/**
 * handshake.c
 * 
 * Implementation of DH Handshake over ICMP
 */

#include "handshake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initialize handshake context
 */
int handshake_init(handshake_context_t *ctx, uint16_t session_id) {
    if (!ctx) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(handshake_context_t));
    
    // Initialize DH context
    if (dh_init(&ctx->dh_ctx) != 0) {
        return HANDSHAKE_ERROR_DH_INIT;
    }
    
    ctx->state = HANDSHAKE_STATE_INIT;
    ctx->session_id = session_id;
    ctx->timeout_ms = HANDSHAKE_TIMEOUT_MS;
    ctx->max_retries = HANDSHAKE_MAX_RETRIES;
    
    return HANDSHAKE_SUCCESS;
}

/**
 * CLIENT: Start handshake
 */
int handshake_client_start(handshake_context_t *ctx, proto_packet_t *request_packet) {
    if (!ctx || !request_packet) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    if (ctx->state != HANDSHAKE_STATE_INIT) {
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Generate DH keypair
    size_t pubkey_len = 256;
    if (dh_generate_keypair(&ctx->dh_ctx, ctx->client_public_key, &pubkey_len) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_DH_KEYGEN;
    }
    
    // Create HANDSHAKE packet with client public key
    int result = proto_create_handshake(ctx->client_public_key, request_packet);
    if (result != PROTO_SUCCESS) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    ctx->state = HANDSHAKE_STATE_SENT_REQUEST;
    
    return HANDSHAKE_SUCCESS;
}

/**
 * CLIENT: Finish handshake (process server response)
 */
int handshake_client_finish(handshake_context_t *ctx, const proto_packet_t *response_packet) {
    if (!ctx || !response_packet) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    if (ctx->state != HANDSHAKE_STATE_SENT_REQUEST) {
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Parse server public key from HANDSHAKE packet
    if (proto_parse_handshake(response_packet, ctx->server_public_key) != PROTO_SUCCESS) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Compute shared secret and derive session key
    uint8_t shared_secret[256];
    size_t secret_len;
    if (dh_compute_shared_secret(&ctx->dh_ctx, ctx->server_public_key, 256, 
                                  shared_secret, &secret_len) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_DH_COMPUTE;
    }
    
    // Derive session key using HKDF
    const char *info = "ICMP-TUNNEL-SESSION-KEY";
    if (dh_derive_session_key(shared_secret, secret_len, info, strlen(info), ctx->session_key) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_DH_COMPUTE;
    }
    
    ctx->state = HANDSHAKE_STATE_COMPLETED;
    
    // Securely wipe shared secret
    memset(shared_secret, 0, sizeof(shared_secret));
    
    return HANDSHAKE_SUCCESS;
}

/**
 * SERVER: Process client request and generate response
 */
int handshake_server_process(handshake_context_t *ctx, 
                              const proto_packet_t *request_packet,
                              proto_packet_t *response_packet) {
    if (!ctx || !request_packet || !response_packet) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    if (ctx->state != HANDSHAKE_STATE_INIT) {
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Parse client public key from HANDSHAKE packet
    if (proto_parse_handshake(request_packet, ctx->client_public_key) != PROTO_SUCCESS) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Generate server DH keypair
    size_t pubkey_len = 256;
    if (dh_generate_keypair(&ctx->dh_ctx, ctx->server_public_key, &pubkey_len) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_DH_KEYGEN;
    }
    
    // Debug: log what was generated
    printf("[DEBUG] Server generated pubkey (first 8): %02X %02X %02X %02X %02X %02X %02X %02X\n",
           ctx->server_public_key[0], ctx->server_public_key[1], ctx->server_public_key[2], ctx->server_public_key[3],
           ctx->server_public_key[4], ctx->server_public_key[5], ctx->server_public_key[6], ctx->server_public_key[7]);
    printf("[DEBUG] Client pubkey (first 8): %02X %02X %02X %02X %02X %02X %02X %02X\n",
           ctx->client_public_key[0], ctx->client_public_key[1], ctx->client_public_key[2], ctx->client_public_key[3],
           ctx->client_public_key[4], ctx->client_public_key[5], ctx->client_public_key[6], ctx->client_public_key[7]);
    
    // Compute shared secret and derive session key
    uint8_t shared_secret[256];
    size_t secret_len;
    if (dh_compute_shared_secret(&ctx->dh_ctx, ctx->client_public_key, 256,
                                  shared_secret, &secret_len) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_DH_COMPUTE;
    }
    
    // Derive session key using HKDF
    const char *info = "ICMP-TUNNEL-SESSION-KEY";
    if (dh_derive_session_key(shared_secret, secret_len, info, strlen(info), ctx->session_key) != 0) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        memset(shared_secret, 0, sizeof(shared_secret));
        return HANDSHAKE_ERROR_DH_COMPUTE;
    }
    
    // Securely wipe shared secret
    memset(shared_secret, 0, sizeof(shared_secret));
    
    // Create HANDSHAKE response packet with server public key
    if (proto_create_handshake(ctx->server_public_key, response_packet) != PROTO_SUCCESS) {
        ctx->state = HANDSHAKE_STATE_FAILED;
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    // Debug: verify what's in the response packet
    printf("[DEBUG] Response packet payload (first 8): %02X %02X %02X %02X %02X %02X %02X %02X\n",
           response_packet->payload[0], response_packet->payload[1], response_packet->payload[2], response_packet->payload[3],
           response_packet->payload[4], response_packet->payload[5], response_packet->payload[6], response_packet->payload[7]);
    
    ctx->state = HANDSHAKE_STATE_COMPLETED;
    
    return HANDSHAKE_SUCCESS;
}

/**
 * Get derived session key
 */
int handshake_get_session_key(const handshake_context_t *ctx, uint8_t session_key[32]) {
    if (!ctx || !session_key) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    if (ctx->state != HANDSHAKE_STATE_COMPLETED) {
        return HANDSHAKE_ERROR_PROTOCOL;
    }
    
    memcpy(session_key, ctx->session_key, 32);
    
    return HANDSHAKE_SUCCESS;
}

/**
 * Check if handshake is complete
 */
int handshake_is_complete(const handshake_context_t *ctx) {
    if (!ctx) return 0;
    return (ctx->state == HANDSHAKE_STATE_COMPLETED) ? 1 : 0;
}

/**
 * Set handshake timeout
 */
int handshake_set_timeout(handshake_context_t *ctx, uint32_t timeout_ms, uint8_t max_retries) {
    if (!ctx) {
        return HANDSHAKE_ERROR_INVALID_PARAM;
    }
    
    if (timeout_ms > 0) {
        ctx->timeout_ms = timeout_ms;
    }
    if (max_retries > 0) {
        ctx->max_retries = max_retries;
    }
    
    return HANDSHAKE_SUCCESS;
}

/**
 * Cleanup handshake context
 */
void handshake_cleanup(handshake_context_t *ctx) {
    if (!ctx) return;
    
    // Cleanup DH context
    dh_cleanup(&ctx->dh_ctx);
    
    // Securely wipe sensitive data
    memset(ctx->session_key, 0, sizeof(ctx->session_key));
    memset(ctx->client_public_key, 0, sizeof(ctx->client_public_key));
    memset(ctx->server_public_key, 0, sizeof(ctx->server_public_key));
    
    ctx->state = HANDSHAKE_STATE_INIT;
}

/**
 * Get error message string
 */
const char* handshake_error_string(int error_code) {
    switch (error_code) {
        case HANDSHAKE_SUCCESS:
            return "Success";
        case HANDSHAKE_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case HANDSHAKE_ERROR_DH_INIT:
            return "DH initialization failed";
        case HANDSHAKE_ERROR_DH_KEYGEN:
            return "DH keypair generation failed";
        case HANDSHAKE_ERROR_DH_COMPUTE:
            return "DH shared secret computation failed";
        case HANDSHAKE_ERROR_SEND_FAILED:
            return "Send failed";
        case HANDSHAKE_ERROR_RECV_FAILED:
            return "Receive failed";
        case HANDSHAKE_ERROR_TIMEOUT:
            return "Timeout";
        case HANDSHAKE_ERROR_PROTOCOL:
            return "Protocol error";
        default:
            return "Unknown error";
    }
}

/**
 * Get state string
 */
const char* handshake_state_string(handshake_state_t state) {
    switch (state) {
        case HANDSHAKE_STATE_INIT:
            return "INIT";
        case HANDSHAKE_STATE_SENT_REQUEST:
            return "SENT_REQUEST";
        case HANDSHAKE_STATE_SENT_RESPONSE:
            return "SENT_RESPONSE";
        case HANDSHAKE_STATE_COMPLETED:
            return "COMPLETED";
        case HANDSHAKE_STATE_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}
