/**
 * aes_gcm.h
 * 
 * AES-256-GCM encryption/decryption for ICMP tunnel payloads
 * 
 * Features:
 * - AES-256 (256-bit key)
 * - GCM mode (Galois/Counter Mode)
 * - 96-bit IV (nonce)
 * - 128-bit authentication tag
 * - Associated data support for session_id + sequence
 */

#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


// Constants
#define AES_GCM_KEY_SIZE 32      // 256 bits
#define AES_GCM_IV_SIZE 12       // 96 bits (recommended for GCM)
#define AES_GCM_TAG_SIZE 16      // 128 bits

// Error codes
#define AES_GCM_SUCCESS 0
#define AES_GCM_ERROR_INVALID_PARAM -1
#define AES_GCM_ERROR_ENCRYPT_FAILED -2
#define AES_GCM_ERROR_DECRYPT_FAILED -3
#define AES_GCM_ERROR_AUTH_FAILED -4
#define AES_GCM_ERROR_MEMORY -5

/**
 * Encrypt data using AES-256-GCM
 * 
 * @param key           Encryption key (32 bytes)
 * @param key_len       Length of key (must be 32)
 * @param iv            Initialization vector (12 bytes, must be unique per encryption)
 * @param iv_len        Length of IV (must be 12)
 * @param plaintext     Data to encrypt
 * @param plaintext_len Length of plaintext
 * @param aad           Additional authenticated data (optional, can be NULL)
 * @param aad_len       Length of AAD (0 if aad is NULL)
 * @param ciphertext    Output buffer for encrypted data (must be >= plaintext_len)
 * @param tag           Output buffer for authentication tag (16 bytes)
 * 
 * @return AES_GCM_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   uint8_t key[32], iv[12], tag[16];
 *   uint8_t plaintext[100], ciphertext[100];
 *   uint8_t aad[4] = {session_id >> 8, session_id & 0xFF, seq >> 8, seq & 0xFF};
 *   
 *   aes_gcm_encrypt(key, 32, iv, 12, plaintext, 100, aad, 4, ciphertext, tag);
 */
int aes_gcm_encrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *plaintext, size_t plaintext_len,
    const uint8_t *aad, size_t aad_len,
    uint8_t *ciphertext,
    uint8_t *tag
);

/**
 * Decrypt data using AES-256-GCM
 * 
 * @param key            Decryption key (32 bytes)
 * @param key_len        Length of key (must be 32)
 * @param iv             Initialization vector (12 bytes)
 * @param iv_len         Length of IV (must be 12)
 * @param ciphertext     Data to decrypt
 * @param ciphertext_len Length of ciphertext
 * @param aad            Additional authenticated data (must match encryption)
 * @param aad_len        Length of AAD
 * @param tag            Authentication tag to verify (16 bytes)
 * @param plaintext      Output buffer for decrypted data (must be >= ciphertext_len)
 * 
 * @return AES_GCM_SUCCESS on success, AES_GCM_ERROR_AUTH_FAILED if tag invalid, error code otherwise
 * 
 * Example:
 *   uint8_t key[32], iv[12], tag[16];
 *   uint8_t ciphertext[100], plaintext[100];
 *   uint8_t aad[4] = {session_id >> 8, session_id & 0xFF, seq >> 8, seq & 0xFF};
 *   
 *   int result = aes_gcm_decrypt(key, 32, iv, 12, ciphertext, 100, aad, 4, tag, plaintext);
 *   if (result == AES_GCM_ERROR_AUTH_FAILED) {
 *       // Authentication failed - data corrupted or wrong key
 *   }
 */
int aes_gcm_decrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *tag,
    uint8_t *plaintext
);

/**
 * Generate a random IV (nonce) for AES-GCM
 * 
 * @param iv Output buffer for IV (12 bytes)
 * 
 * @return AES_GCM_SUCCESS on success, error code otherwise
 * 
 * Note: IV must be unique for each encryption with the same key.
 *       This function uses cryptographically secure random generation.
 */
int aes_gcm_generate_iv(uint8_t *iv);

/**
 * Generate a random key for AES-256
 * 
 * @param key Output buffer for key (32 bytes)
 * 
 * @return AES_GCM_SUCCESS on success, error code otherwise
 */
int aes_gcm_generate_key(uint8_t *key);

/**
 * Get error message string for error code
 * 
 * @param error_code Error code from encrypt/decrypt functions
 * @return Human-readable error message
 */
const char* aes_gcm_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif // AES_GCM_H
