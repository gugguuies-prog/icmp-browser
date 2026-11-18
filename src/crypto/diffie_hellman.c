/**
 * diffie_hellman.c
 * 
 * Implementation of Diffie-Hellman key exchange using OpenSSL
 */

#include "diffie_hellman.h"
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <string.h>
#include <stdio.h>

// RFC 3526 MODP Group 14 (2048-bit)
// https://www.rfc-editor.org/rfc/rfc3526#section-3
static const char *RFC3526_PRIME_2048 =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
    "15728E5A8AACAA68FFFFFFFFFFFFFFFF";

static const char *RFC3526_GENERATOR = "2";

// Fixed salt for HKDF
static const char *HKDF_SALT = "ICMP_TUNNEL_SALT_v1";

/**
 * Initialize Diffie-Hellman context
 */
int dh_init(dh_context_t *ctx) {
    if (!ctx) {
        return DH_ERROR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(dh_context_t));

    // Create DH object
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY *params = NULL;
    BIGNUM *p = NULL, *g = NULL;
    DH *dh = NULL;
    int ret = DH_SUCCESS;

    // Convert hex strings to BIGNUMs
    if (BN_hex2bn(&p, RFC3526_PRIME_2048) == 0) {
        ret = DH_ERROR_KEYGEN_FAILED;
        goto cleanup;
    }
    if (BN_hex2bn(&g, RFC3526_GENERATOR) == 0) {
        ret = DH_ERROR_KEYGEN_FAILED;
        goto cleanup;
    }

    // Create DH object and set parameters
    dh = DH_new();
    if (!dh || !DH_set0_pqg(dh, p, NULL, g)) {
        ret = DH_ERROR_KEYGEN_FAILED;
        goto cleanup;
    }
    p = NULL; // Ownership transferred
    g = NULL; // Ownership transferred

    // Store DH context (will generate keys later)
    ctx->dh_ctx = dh;
    ctx->initialized = 1;
    dh = NULL; // Prevent double-free

cleanup:
    if (p) BN_free(p);
    if (g) BN_free(g);
    if (dh) DH_free(dh);
    if (pctx) EVP_PKEY_CTX_free(pctx);
    if (params) EVP_PKEY_free(params);

    return ret;
}

/**
 * Generate DH keypair
 */
int dh_generate_keypair(dh_context_t *ctx, uint8_t *public_key, size_t *pubkey_len) {
    if (!ctx || !ctx->initialized || !ctx->dh_ctx || !public_key || !pubkey_len) {
        return DH_ERROR_INVALID_PARAM;
    }

    DH *dh = (DH *)ctx->dh_ctx;
    const BIGNUM *pub_key_bn = NULL;
    const BIGNUM *priv_key_bn = NULL;
    int ret = DH_SUCCESS;

    // Generate keypair
    if (DH_generate_key(dh) != 1) {
        return DH_ERROR_KEYGEN_FAILED;
    }

    // Get public and private keys
    DH_get0_key(dh, &pub_key_bn, &priv_key_bn);
    if (!pub_key_bn || !priv_key_bn) {
        return DH_ERROR_KEYGEN_FAILED;
    }

    // Convert public key BIGNUM to bytes
    int pub_len = BN_num_bytes(pub_key_bn);
    if (pub_len > DH_PUBKEY_SIZE) {
        return DH_ERROR_KEYGEN_FAILED;
    }
    
    *pubkey_len = pub_len;
    BN_bn2bin(pub_key_bn, public_key);

    // Store public key in context
    memcpy(ctx->public_key, public_key, pub_len);
    ctx->public_key_len = pub_len;

    // Store private key in context (for reference, but OpenSSL manages it)
    int priv_len = BN_num_bytes(priv_key_bn);
    if (priv_len <= DH_KEY_SIZE) {
        BN_bn2bin(priv_key_bn, ctx->private_key);
    }

    return ret;
}

/**
 * Compute shared secret from peer's public key
 */
int dh_compute_shared_secret(
    dh_context_t *ctx,
    const uint8_t *peer_pubkey,
    size_t peer_pubkey_len,
    uint8_t *shared_secret,
    size_t *secret_len
) {
    if (!ctx || !ctx->initialized || !ctx->dh_ctx ||
        !peer_pubkey || peer_pubkey_len == 0 ||
        !shared_secret || !secret_len) {
        return DH_ERROR_INVALID_PARAM;
    }

    DH *dh = (DH *)ctx->dh_ctx;
    BIGNUM *peer_pub_bn = NULL;
    int ret = DH_SUCCESS;
    int computed_len;

    // Convert peer's public key bytes to BIGNUM
    peer_pub_bn = BN_bin2bn(peer_pubkey, peer_pubkey_len, NULL);
    if (!peer_pub_bn) {
        return DH_ERROR_COMPUTE_FAILED;
    }

    // Compute shared secret
    computed_len = DH_compute_key(shared_secret, peer_pub_bn, dh);
    if (computed_len < 0) {
        ret = DH_ERROR_COMPUTE_FAILED;
        goto cleanup;
    }

    *secret_len = computed_len;

cleanup:
    if (peer_pub_bn) {
        BN_free(peer_pub_bn);
    }

    return ret;
}

/**
 * Derive session key from shared secret using HKDF-SHA256
 */
int dh_derive_session_key(
    const uint8_t *shared_secret,
    size_t secret_len,
    const char *info,
    size_t info_len,
    uint8_t *session_key
) {
    if (!shared_secret || secret_len == 0 || !session_key) {
        return DH_ERROR_INVALID_PARAM;
    }

    EVP_PKEY_CTX *pctx = NULL;
    int ret = DH_SUCCESS;

    // Create HKDF context
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (!pctx) {
        return DH_ERROR_DERIVE_FAILED;
    }

    // Initialize HKDF
    if (EVP_PKEY_derive_init(pctx) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    // Set HKDF mode to extract-and-expand
    if (EVP_PKEY_CTX_set_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    // Set hash function to SHA-256
    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    // Set salt
    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, (const unsigned char *)HKDF_SALT, 
                                     strlen(HKDF_SALT)) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    // Set input key material (shared secret)
    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, shared_secret, secret_len) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    // Set info (context string)
    if (info && info_len > 0) {
        if (EVP_PKEY_CTX_add1_hkdf_info(pctx, (const unsigned char *)info, 
                                         info_len) <= 0) {
            ret = DH_ERROR_DERIVE_FAILED;
            goto cleanup;
        }
    }

    // Derive session key (32 bytes for AES-256)
    size_t outlen = SESSION_KEY_SIZE;
    if (EVP_PKEY_derive(pctx, session_key, &outlen) <= 0) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

    if (outlen != SESSION_KEY_SIZE) {
        ret = DH_ERROR_DERIVE_FAILED;
        goto cleanup;
    }

cleanup:
    if (pctx) {
        EVP_PKEY_CTX_free(pctx);
    }

    return ret;
}

/**
 * Complete DH handshake (convenience function)
 */
int dh_complete_handshake(
    dh_context_t *ctx,
    uint8_t *local_pubkey,
    size_t *local_pubkey_len,
    const uint8_t *peer_pubkey,
    size_t peer_pubkey_len,
    uint8_t *session_key
) {
    if (!ctx || !peer_pubkey || peer_pubkey_len == 0 || !session_key) {
        return DH_ERROR_INVALID_PARAM;
    }

    uint8_t shared_secret[DH_SHARED_SECRET_SIZE];
    size_t secret_len;
    int ret;

    // Step 1: Generate our keypair (if not already done)
    if (local_pubkey && local_pubkey_len) {
        ret = dh_generate_keypair(ctx, local_pubkey, local_pubkey_len);
        if (ret != DH_SUCCESS) {
            return ret;
        }
    }

    // Step 2: Compute shared secret
    ret = dh_compute_shared_secret(ctx, peer_pubkey, peer_pubkey_len, 
                                     shared_secret, &secret_len);
    if (ret != DH_SUCCESS) {
        return ret;
    }

    // Step 3: Derive session key
    ret = dh_derive_session_key(shared_secret, secret_len, 
                                  "ICMP_TUNNEL_v1", 14, 
                                  session_key);

    // Securely zero shared secret
    memset(shared_secret, 0, sizeof(shared_secret));

    return ret;
}

/**
 * Clean up DH context
 */
int dh_cleanup(dh_context_t *ctx) {
    if (!ctx) {
        return DH_ERROR_INVALID_PARAM;
    }

    if (ctx->dh_ctx) {
        DH_free((DH *)ctx->dh_ctx);
        ctx->dh_ctx = NULL;
    }

    // Securely zero private key
    memset(ctx->private_key, 0, sizeof(ctx->private_key));
    memset(ctx, 0, sizeof(dh_context_t));

    return DH_SUCCESS;
}

/**
 * Get error message string
 */
const char* dh_error_string(int error_code) {
    switch (error_code) {
        case DH_SUCCESS:
            return "Success";
        case DH_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case DH_ERROR_KEYGEN_FAILED:
            return "Key generation failed";
        case DH_ERROR_COMPUTE_FAILED:
            return "Shared secret computation failed";
        case DH_ERROR_DERIVE_FAILED:
            return "Session key derivation failed";
        case DH_ERROR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}
