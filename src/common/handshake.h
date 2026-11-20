/**
 * handshake.h
 * 
 * Diffie-Hellman Handshake over ICMP
 * 
 * Handshake Flow:
 * 1. Client: Generate DH keypair
 * 2. Client → Server: HANDSHAKE packet with client public key
 * 3. Server: Generate DH keypair
 * 4. Server → Client: HANDSHAKE packet with server public key
 * 5. Both: Compute shared secret → derive session key (AES-256)
 * 
 * Features:
 * - DH-2048 key exchange
 * - HKDF-SHA256 for session key derivation
 * - Timeout and retry support
 * - Thread-safe
 */

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "../crypto/diffie_hellman.h"
#include "protocol.h"

// Constants
#define HANDSHAKE_TIMEOUT_MS 5000        // Timeout for handshake (5 seconds)
#define HANDSHAKE_MAX_RETRIES 3          // Maximum retry attempts
#define HANDSHAKE_SESSION_KEY_SIZE 32    // AES-256 key size

// Error codes
#define HANDSHAKE_SUCCESS 0
#define HANDSHAKE_ERROR_INVALID_PARAM -1
#define HANDSHAKE_ERROR_DH_INIT -2
#define HANDSHAKE_ERROR_DH_KEYGEN -3
#define HANDSHAKE_ERROR_DH_COMPUTE -4
#define HANDSHAKE_ERROR_SEND_FAILED -5
#define HANDSHAKE_ERROR_RECV_FAILED -6
#define HANDSHAKE_ERROR_TIMEOUT -7
#define HANDSHAKE_ERROR_PROTOCOL -8

/**
 * Handshake state
 */
typedef enum {
    HANDSHAKE_STATE_INIT,                // Initial state
    HANDSHAKE_STATE_SENT_REQUEST,        // Client sent request
    HANDSHAKE_STATE_SENT_RESPONSE,       // Server sent response
    HANDSHAKE_STATE_COMPLETED,           // Handshake completed
    HANDSHAKE_STATE_FAILED               // Handshake failed
} handshake_state_t;

/**
 * Handshake context
 */
typedef struct {
    dh_context_t dh_ctx;                 // DH context
    handshake_state_t state;             // Current state
    uint8_t session_key[32];             // Derived session key (AES-256)
    uint16_t session_id;                 // Session ID
    uint8_t client_public_key[256];      // Client DH public key
    uint8_t server_public_key[256];      // Server DH public key
    uint32_t timeout_ms;                 // Timeout in milliseconds
    uint8_t max_retries;                 // Maximum retry attempts
} handshake_context_t;

/**
 * Initialize handshake context
 * 
 * @param ctx Handshake context
 * @param session_id Session ID for this handshake
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   handshake_context_t ctx;
 *   handshake_init(&ctx, 0x1234);
 */
int handshake_init(handshake_context_t *ctx, uint16_t session_id);

/**
 * CLIENT: Start handshake (generate request packet)
 * 
 * @param ctx Handshake context
 * @param request_packet Output: HANDSHAKE request packet
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 * 
 * Note: Generates DH keypair and creates HANDSHAKE packet with client public key
 * 
 * Example:
 *   proto_packet_t request;
 *   handshake_client_start(&ctx, &request);
 *   // Send request via ICMP
 */
int handshake_client_start(handshake_context_t *ctx, proto_packet_t *request_packet);

/**
 * CLIENT: Process server response and derive session key
 * 
 * @param ctx Handshake context
 * @param response_packet Server HANDSHAKE response packet
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 * 
 * Note: Extracts server public key, computes shared secret, derives session key
 * 
 * Example:
 *   proto_packet_t response; // Received from server
 *   handshake_client_finish(&ctx, &response);
 *   // ctx.session_key now contains AES-256 key
 */
int handshake_client_finish(handshake_context_t *ctx, const proto_packet_t *response_packet);

/**
 * SERVER: Process client request and generate response
 * 
 * @param ctx Handshake context
 * @param request_packet Client HANDSHAKE request packet
 * @param response_packet Output: HANDSHAKE response packet
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 * 
 * Note: Extracts client public key, generates server keypair,
 *       computes shared secret, derives session key, creates response
 * 
 * Example:
 *   proto_packet_t request;  // Received from client
 *   proto_packet_t response;
 *   handshake_server_process(&ctx, &request, &response);
 *   // Send response via ICMP
 *   // ctx.session_key now contains AES-256 key
 */
int handshake_server_process(handshake_context_t *ctx, 
                              const proto_packet_t *request_packet,
                              proto_packet_t *response_packet);

/**
 * Get derived session key
 * 
 * @param ctx Handshake context
 * @param session_key Output: session key (32 bytes)
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 * 
 * Note: Only valid after handshake completion
 * 
 * Example:
 *   uint8_t key[32];
 *   handshake_get_session_key(&ctx, key);
 */
int handshake_get_session_key(const handshake_context_t *ctx, uint8_t session_key[32]);

/**
 * Check if handshake is complete
 * 
 * @param ctx Handshake context
 * @return 1 if complete, 0 otherwise
 */
int handshake_is_complete(const handshake_context_t *ctx);

/**
 * Set handshake timeout
 * 
 * @param ctx Handshake context
 * @param timeout_ms Timeout in milliseconds
 * @param max_retries Maximum retry attempts
 * 
 * @return HANDSHAKE_SUCCESS on success, error code otherwise
 */
int handshake_set_timeout(handshake_context_t *ctx, uint32_t timeout_ms, uint8_t max_retries);

/**
 * Cleanup handshake context
 * 
 * @param ctx Handshake context
 * 
 * Note: Securely wipes sensitive data (keys, shared secrets)
 */
void handshake_cleanup(handshake_context_t *ctx);

/**
 * Get error message string
 * 
 * @param error_code Error code
 * @return Human-readable error message
 */
const char* handshake_error_string(int error_code);

/**
 * Get state string
 * 
 * @param state Handshake state
 * @return Human-readable state string
 */
const char* handshake_state_string(handshake_state_t state);

#ifdef __cplusplus
}
#endif

#endif // HANDSHAKE_H
