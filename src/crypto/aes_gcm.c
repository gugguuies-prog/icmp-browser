/**
 * aes_gcm.c
 * 
 * Implementation of AES-256-GCM encryption/decryption using OpenSSL
 */

#include "aes_gcm.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <string.h>
#include <stdio.h>

/**
 * Encrypt data using AES-256-GCM
 */
int aes_gcm_encrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *plaintext, size_t plaintext_len,
    const uint8_t *aad, size_t aad_len,
    uint8_t *ciphertext,
    uint8_t *tag
) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len;
    int ciphertext_len;
    int ret = AES_GCM_SUCCESS;

    // Validate parameters
    if (!key || key_len != AES_GCM_KEY_SIZE) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (!iv || iv_len != AES_GCM_IV_SIZE) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (!plaintext || plaintext_len == 0 || !ciphertext || !tag) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (aad_len > 0 && !aad) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }

    // Create and initialize the context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return AES_GCM_ERROR_MEMORY;
    }

    // Initialize encryption operation with AES-256-GCM
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }

    // Set IV length (12 bytes for GCM)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }

    // Initialize key and IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }

    // Provide AAD (Additional Authenticated Data) if present
    if (aad && aad_len > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            ret = AES_GCM_ERROR_ENCRYPT_FAILED;
            goto cleanup;
        }
    }

    // Encrypt plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }
    ciphertext_len = len;

    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }
    ciphertext_len += len;

    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, tag) != 1) {
        ret = AES_GCM_ERROR_ENCRYPT_FAILED;
        goto cleanup;
    }

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    return ret;
}

/**
 * Decrypt data using AES-256-GCM
 */
int aes_gcm_decrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *ciphertext, size_t ciphertext_len,
    const uint8_t *aad, size_t aad_len,
    const uint8_t *tag,
    uint8_t *plaintext
) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len;
    int plaintext_len;
    int ret = AES_GCM_SUCCESS;

    // Validate parameters
    if (!key || key_len != AES_GCM_KEY_SIZE) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (!iv || iv_len != AES_GCM_IV_SIZE) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (!ciphertext || ciphertext_len == 0 || !plaintext || !tag) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }
    if (aad_len > 0 && !aad) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }

    // Create and initialize the context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return AES_GCM_ERROR_MEMORY;
    }

    // Initialize decryption operation with AES-256-GCM
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        ret = AES_GCM_ERROR_DECRYPT_FAILED;
        goto cleanup;
    }

    // Set IV length (12 bytes for GCM)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL) != 1) {
        ret = AES_GCM_ERROR_DECRYPT_FAILED;
        goto cleanup;
    }

    // Initialize key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        ret = AES_GCM_ERROR_DECRYPT_FAILED;
        goto cleanup;
    }

    // Provide AAD if present
    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            ret = AES_GCM_ERROR_DECRYPT_FAILED;
            goto cleanup;
        }
    }

    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        ret = AES_GCM_ERROR_DECRYPT_FAILED;
        goto cleanup;
    }
    plaintext_len = len;

    // Set expected tag value
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, (void*)tag) != 1) {
        ret = AES_GCM_ERROR_DECRYPT_FAILED;
        goto cleanup;
    }

    // Finalize decryption and verify tag
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        // Tag verification failed
        ret = AES_GCM_ERROR_AUTH_FAILED;
        goto cleanup;
    }
    plaintext_len += len;

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }
    return ret;
}

/**
 * Generate a random IV (nonce) for AES-GCM
 */
int aes_gcm_generate_iv(uint8_t *iv) {
    if (!iv) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }

    // Use OpenSSL's cryptographically secure random generator
    if (RAND_bytes(iv, AES_GCM_IV_SIZE) != 1) {
        return AES_GCM_ERROR_ENCRYPT_FAILED;
    }

    return AES_GCM_SUCCESS;
}

/**
 * Generate a random key for AES-256
 */
int aes_gcm_generate_key(uint8_t *key) {
    if (!key) {
        return AES_GCM_ERROR_INVALID_PARAM;
    }

    // Use OpenSSL's cryptographically secure random generator
    if (RAND_bytes(key, AES_GCM_KEY_SIZE) != 1) {
        return AES_GCM_ERROR_ENCRYPT_FAILED;
    }

    return AES_GCM_SUCCESS;
}

/**
 * Get error message string for error code
 */
const char* aes_gcm_error_string(int error_code) {
    switch (error_code) {
        case AES_GCM_SUCCESS:
            return "Success";
        case AES_GCM_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case AES_GCM_ERROR_ENCRYPT_FAILED:
            return "Encryption failed";
        case AES_GCM_ERROR_DECRYPT_FAILED:
            return "Decryption failed";
        case AES_GCM_ERROR_AUTH_FAILED:
            return "Authentication failed - tag verification error";
        case AES_GCM_ERROR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}
