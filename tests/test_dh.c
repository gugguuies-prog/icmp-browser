/**
 * test_dh.c
 * 
 * Unit tests for Diffie-Hellman key exchange
 */

#include "../src/crypto/diffie_hellman.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test colors
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

// Helper function to print hex
void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    if (len > 64) {
        // Print first 32 bytes
        for (size_t i = 0; i < 32; i++) {
            printf("%02X ", data[i]);
        }
        printf("... ");
        // Print last 32 bytes
        for (size_t i = len - 32; i < len; i++) {
            printf("%02X ", data[i]);
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            printf("%02X ", data[i]);
        }
    }
    printf("\n");
}

// Test 1: DH initialization
void test_dh_init() {
    printf("\n=== Test 1: DH Initialization ===\n");
    
    dh_context_t ctx;
    int result = dh_init(&ctx);
    
    assert(result == DH_SUCCESS);
    assert(ctx.initialized == 1);
    assert(ctx.dh_ctx != NULL);
    
    printf("✓ DH context initialized\n");
    printf("✓ RFC 3526 MODP Group 14 (2048-bit) loaded\n");
    
    dh_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Keypair generation
void test_keypair_generation() {
    printf("\n=== Test 2: Keypair Generation ===\n");
    
    dh_context_t ctx;
    uint8_t public_key[DH_PUBKEY_SIZE];
    size_t pubkey_len;
    
    dh_init(&ctx);
    
    int result = dh_generate_keypair(&ctx, public_key, &pubkey_len);
    
    assert(result == DH_SUCCESS);
    assert(pubkey_len > 0 && pubkey_len <= DH_PUBKEY_SIZE);
    
    printf("✓ Keypair generated successfully\n");
    printf("Public key length: %zu bytes\n", pubkey_len);
    print_hex("Public key", public_key, pubkey_len);
    
    // Verify public key is stored in context
    assert(ctx.public_key_len == pubkey_len);
    assert(memcmp(ctx.public_key, public_key, pubkey_len) == 0);
    printf("✓ Public key stored in context\n");
    
    dh_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
}

// Test 3: Complete DH exchange (client-server)
void test_complete_dh_exchange() {
    printf("\n=== Test 3: Complete DH Exchange (Client-Server) ===\n");
    
    dh_context_t client_ctx, server_ctx;
    uint8_t client_pubkey[DH_PUBKEY_SIZE], server_pubkey[DH_PUBKEY_SIZE];
    size_t client_pubkey_len, server_pubkey_len;
    uint8_t client_session_key[SESSION_KEY_SIZE];
    uint8_t server_session_key[SESSION_KEY_SIZE];
    
    // Initialize both contexts
    assert(dh_init(&client_ctx) == DH_SUCCESS);
    assert(dh_init(&server_ctx) == DH_SUCCESS);
    printf("✓ Client and server DH contexts initialized\n");
    
    // Step 1: Client generates keypair
    assert(dh_generate_keypair(&client_ctx, client_pubkey, &client_pubkey_len) == DH_SUCCESS);
    printf("✓ Client generated keypair\n");
    print_hex("Client public key", client_pubkey, client_pubkey_len);
    
    // Step 2: Server generates keypair
    assert(dh_generate_keypair(&server_ctx, server_pubkey, &server_pubkey_len) == DH_SUCCESS);
    printf("✓ Server generated keypair\n");
    print_hex("Server public key", server_pubkey, server_pubkey_len);
    
    // Step 3: Client computes shared secret from server's public key
    uint8_t client_shared_secret[DH_SHARED_SECRET_SIZE];
    size_t client_secret_len;
    assert(dh_compute_shared_secret(&client_ctx, server_pubkey, server_pubkey_len,
                                     client_shared_secret, &client_secret_len) == DH_SUCCESS);
    printf("✓ Client computed shared secret (%zu bytes)\n", client_secret_len);
    print_hex("Client shared secret", client_shared_secret, client_secret_len);
    
    // Step 4: Server computes shared secret from client's public key
    uint8_t server_shared_secret[DH_SHARED_SECRET_SIZE];
    size_t server_secret_len;
    assert(dh_compute_shared_secret(&server_ctx, client_pubkey, client_pubkey_len,
                                     server_shared_secret, &server_secret_len) == DH_SUCCESS);
    printf("✓ Server computed shared secret (%zu bytes)\n", server_secret_len);
    print_hex("Server shared secret", server_shared_secret, server_secret_len);
    
    // Step 5: Verify shared secrets match
    assert(client_secret_len == server_secret_len);
    assert(memcmp(client_shared_secret, server_shared_secret, client_secret_len) == 0);
    printf("✓✓✓ Shared secrets MATCH! DH exchange successful\n");
    
    // Step 6: Client derives session key
    assert(dh_derive_session_key(client_shared_secret, client_secret_len,
                                  "ICMP_TUNNEL_v1", 14,
                                  client_session_key) == DH_SUCCESS);
    printf("✓ Client derived session key\n");
    print_hex("Client session key", client_session_key, SESSION_KEY_SIZE);
    
    // Step 7: Server derives session key
    assert(dh_derive_session_key(server_shared_secret, server_secret_len,
                                  "ICMP_TUNNEL_v1", 14,
                                  server_session_key) == DH_SUCCESS);
    printf("✓ Server derived session key\n");
    print_hex("Server session key", server_session_key, SESSION_KEY_SIZE);
    
    // Step 8: Verify session keys match
    assert(memcmp(client_session_key, server_session_key, SESSION_KEY_SIZE) == 0);
    printf("✓✓✓ Session keys MATCH! Ready for AES-256-GCM encryption\n");
    
    // Cleanup
    dh_cleanup(&client_ctx);
    dh_cleanup(&server_ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 3 PASSED\n" COLOR_RESET);
}

// Test 4: Complete handshake convenience function
void test_complete_handshake_function() {
    printf("\n=== Test 4: Complete Handshake (Convenience Function) ===\n");
    
    dh_context_t client_ctx, server_ctx;
    uint8_t client_pubkey[DH_PUBKEY_SIZE], server_pubkey[DH_PUBKEY_SIZE];
    size_t client_pubkey_len, server_pubkey_len;
    uint8_t client_session_key[SESSION_KEY_SIZE];
    uint8_t server_session_key[SESSION_KEY_SIZE];
    
    // Initialize contexts
    dh_init(&client_ctx);
    dh_init(&server_ctx);
    
    // Client: Generate keypair
    dh_generate_keypair(&client_ctx, client_pubkey, &client_pubkey_len);
    printf("✓ Client generated keypair\n");
    
    // Server: Generate keypair
    dh_generate_keypair(&server_ctx, server_pubkey, &server_pubkey_len);
    printf("✓ Server generated keypair\n");
    
    // Client: Complete handshake (compute secret + derive key)
    int result = dh_complete_handshake(&client_ctx, NULL, NULL,
                                        server_pubkey, server_pubkey_len,
                                        client_session_key);
    assert(result == DH_SUCCESS);
    printf("✓ Client completed handshake\n");
    print_hex("Client session key", client_session_key, SESSION_KEY_SIZE);
    
    // Server: Complete handshake
    result = dh_complete_handshake(&server_ctx, NULL, NULL,
                                    client_pubkey, client_pubkey_len,
                                    server_session_key);
    assert(result == DH_SUCCESS);
    printf("✓ Server completed handshake\n");
    print_hex("Server session key", server_session_key, SESSION_KEY_SIZE);
    
    // Verify session keys match
    assert(memcmp(client_session_key, server_session_key, SESSION_KEY_SIZE) == 0);
    printf("✓✓✓ Session keys MATCH!\n");
    
    // Cleanup
    dh_cleanup(&client_ctx);
    dh_cleanup(&server_ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
}

// Test 5: HKDF with different info strings
void test_hkdf_different_info() {
    printf("\n=== Test 5: HKDF with Different Info Strings ===\n");
    
    // Same shared secret
    uint8_t shared_secret[32];
    memset(shared_secret, 0xAB, 32);
    
    uint8_t key1[SESSION_KEY_SIZE];
    uint8_t key2[SESSION_KEY_SIZE];
    uint8_t key3[SESSION_KEY_SIZE];
    
    // Derive with different info strings
    dh_derive_session_key(shared_secret, 32, "ICMP_TUNNEL_v1", 14, key1);
    dh_derive_session_key(shared_secret, 32, "ICMP_TUNNEL_v2", 14, key2);
    dh_derive_session_key(shared_secret, 32, "ICMP_TUNNEL_v1", 14, key3);
    
    print_hex("Key 1 (v1)", key1, SESSION_KEY_SIZE);
    print_hex("Key 2 (v2)", key2, SESSION_KEY_SIZE);
    print_hex("Key 3 (v1)", key3, SESSION_KEY_SIZE);
    
    // Keys with different info should differ
    assert(memcmp(key1, key2, SESSION_KEY_SIZE) != 0);
    printf("✓ Different info strings produce different keys\n");
    
    // Keys with same info should match
    assert(memcmp(key1, key3, SESSION_KEY_SIZE) == 0);
    printf("✓ Same info strings produce same keys\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Test 6: Error handling - invalid parameters
void test_error_handling() {
    printf("\n=== Test 6: Error Handling ===\n");
    
    dh_context_t ctx;
    uint8_t pubkey[DH_PUBKEY_SIZE];
    size_t pubkey_len;
    
    // Test NULL context initialization
    assert(dh_init(NULL) == DH_ERROR_INVALID_PARAM);
    printf("✓ NULL context rejected in init\n");
    
    // Test keypair generation without initialization
    dh_context_t uninit_ctx;
    memset(&uninit_ctx, 0, sizeof(uninit_ctx));
    assert(dh_generate_keypair(&uninit_ctx, pubkey, &pubkey_len) == DH_ERROR_INVALID_PARAM);
    printf("✓ Uninitialized context rejected in keypair generation\n");
    
    // Test compute with NULL peer key
    dh_init(&ctx);
    dh_generate_keypair(&ctx, pubkey, &pubkey_len);
    uint8_t shared_secret[DH_SHARED_SECRET_SIZE];
    size_t secret_len;
    assert(dh_compute_shared_secret(&ctx, NULL, 0, shared_secret, &secret_len) == DH_ERROR_INVALID_PARAM);
    printf("✓ NULL peer key rejected\n");
    
    dh_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 6 PASSED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Diffie-Hellman Unit Tests                  ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    test_dh_init();
    test_keypair_generation();
    test_complete_dh_exchange();
    test_complete_handshake_function();
    test_hkdf_different_info();
    test_error_handling();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS PASSED ✓" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    return 0;
}
