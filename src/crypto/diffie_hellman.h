/**
 * diffie_hellman.h
 * 
 * Diffie-Hellman key exchange (2048-bit) for ICMP tunnel session key establishment
 * 
 * Features:
 * - DH-2048 (2048-bit modular exponentiation)
 * - RFC 3526 MODP group 14 parameters
 * - HKDF-SHA256 for key derivation
 * - Session key generation (256-bit for AES-256)
 */

#ifndef DIFFIE_HELLMAN_H
#define DIFFIE_HELLMAN_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


// Constants
#define DH_KEY_SIZE 256          // 2048 bits in bytes
#define DH_PUBKEY_SIZE 256       // Public key size (2048 bits)
#define DH_SHARED_SECRET_SIZE 256 // Shared secret size (2048 bits)
#define SESSION_KEY_SIZE 32      // Derived session key (256 bits for AES-256)

// Error codes
#define DH_SUCCESS 0
#define DH_ERROR_INVALID_PARAM -1
#define DH_ERROR_KEYGEN_FAILED -2
#define DH_ERROR_COMPUTE_FAILED -3
#define DH_ERROR_DERIVE_FAILED -4
#define DH_ERROR_MEMORY -5

/**
 * Diffie-Hellman context structure
 * Contains private key and DH parameters
 */
typedef struct {
    void *dh_ctx;                         // OpenSSL DH context (opaque)
    uint8_t private_key[DH_KEY_SIZE];     // Private key (kept secret)
    uint8_t public_key[DH_PUBKEY_SIZE];   // Public key (sent to peer)
    size_t public_key_len;                // Actual length of public key
    int initialized;                       // 1 if initialized, 0 otherwise
} dh_context_t;

/**
 * Initialize Diffie-Hellman context with RFC 3526 MODP Group 14 parameters
 * 
 * @param ctx DH context to initialize
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Note: Uses standard 2048-bit prime from RFC 3526 (MODP Group 14)
 */
int dh_init(dh_context_t *ctx);

/**
 * Generate DH keypair (private + public keys)
 * 
 * @param ctx        DH context (must be initialized)
 * @param public_key Output buffer for public key (256 bytes)
 * @param pubkey_len Output: actual length of public key
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   dh_context_t ctx;
 *   uint8_t public_key[DH_PUBKEY_SIZE];
 *   size_t pubkey_len;
 *   
 *   dh_init(&ctx);
 *   dh_generate_keypair(&ctx, public_key, &pubkey_len);
 *   // Send public_key to peer
 */
int dh_generate_keypair(dh_context_t *ctx, uint8_t *public_key, size_t *pubkey_len);

/**
 * Compute shared secret from peer's public key
 * 
 * @param ctx            DH context (must have generated keypair)
 * @param peer_pubkey    Peer's public key (received from network)
 * @param peer_pubkey_len Length of peer's public key
 * @param shared_secret  Output buffer for shared secret (256 bytes)
 * @param secret_len     Output: actual length of shared secret
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   uint8_t peer_public_key[256];  // Received from peer
 *   uint8_t shared_secret[DH_SHARED_SECRET_SIZE];
 *   size_t secret_len;
 *   
 *   dh_compute_shared_secret(&ctx, peer_public_key, 256, shared_secret, &secret_len);
 */
int dh_compute_shared_secret(
    dh_context_t *ctx,
    const uint8_t *peer_pubkey,
    size_t peer_pubkey_len,
    uint8_t *shared_secret,
    size_t *secret_len
);

/**
 * Derive session key from shared secret using HKDF-SHA256
 * 
 * @param shared_secret Shared secret from DH exchange
 * @param secret_len    Length of shared secret
 * @param info          Context info string (e.g., "ICMP_TUNNEL_v1")
 * @param info_len      Length of info string
 * @param session_key   Output buffer for session key (32 bytes)
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Note: Uses HKDF-SHA256 (HMAC-based Key Derivation Function)
 *       Salt is fixed to "ICMP_TUNNEL_SALT_v1"
 * 
 * Example:
 *   uint8_t shared_secret[256];
 *   uint8_t session_key[SESSION_KEY_SIZE];
 *   
 *   dh_derive_session_key(shared_secret, 256, "ICMP_TUNNEL_v1", 14, session_key);
 *   // Use session_key for AES-256-GCM encryption
 */
int dh_derive_session_key(
    const uint8_t *shared_secret,
    size_t secret_len,
    const char *info,
    size_t info_len,
    uint8_t *session_key
);

/**
 * Complete DH handshake: generate keypair, compute secret, derive session key
 * 
 * @param ctx            DH context (must be initialized)
 * @param local_pubkey   Output: our public key to send to peer
 * @param local_pubkey_len Output: length of our public key
 * @param peer_pubkey    Input: peer's public key (received)
 * @param peer_pubkey_len Input: length of peer's public key
 * @param session_key    Output: derived 256-bit session key
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Convenience function that does:
 * 1. Generate our keypair
 * 2. Compute shared secret from peer's public key
 * 3. Derive session key using HKDF
 * 
 * Example (Client):
 *   dh_context_t ctx;
 *   uint8_t my_pubkey[256], peer_pubkey[256], session_key[32];
 *   size_t my_pubkey_len;
 *   
 *   dh_init(&ctx);
 *   // Step 1: Generate and send our public key
 *   dh_generate_keypair(&ctx, my_pubkey, &my_pubkey_len);
 *   send_to_server(my_pubkey, my_pubkey_len);
 *   
 *   // Step 2: Receive peer's public key
 *   recv_from_server(peer_pubkey, &peer_pubkey_len);
 *   
 *   // Step 3: Complete handshake
 *   dh_complete_handshake(&ctx, NULL, NULL, peer_pubkey, peer_pubkey_len, session_key);
 */
int dh_complete_handshake(
    dh_context_t *ctx,
    uint8_t *local_pubkey,
    size_t *local_pubkey_len,
    const uint8_t *peer_pubkey,
    size_t peer_pubkey_len,
    uint8_t *session_key
);

/**
 * Clean up and free DH context
 * 
 * @param ctx DH context to clean up
 * 
 * @return DH_SUCCESS on success, error code otherwise
 * 
 * Note: Should be called after session key is derived.
 *       Securely zeros private key memory.
 */
int dh_cleanup(dh_context_t *ctx);

/**
 * Get error message string for error code
 * 
 * @param error_code Error code from DH functions
 * @return Human-readable error message
 */
const char* dh_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif // DIFFIE_HELLMAN_H
