/**
 * test_aes_gcm.c
 * 
 * Unit tests for AES-256-GCM encryption/decryption
 */

#include "../src/crypto/aes_gcm.h"
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
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// Test 1: Basic encryption and decryption (round-trip)
void test_basic_encryption_decryption() {
    printf("\n=== Test 1: Basic Encryption/Decryption ===\n");
    
    uint8_t key[AES_GCM_KEY_SIZE];
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    
    const char *plaintext_str = "Hello, ICMP Tunnel! This is a test message.";
    size_t plaintext_len = strlen(plaintext_str);
    uint8_t plaintext[256];
    uint8_t ciphertext[256];
    uint8_t decrypted[256];
    
    memcpy(plaintext, plaintext_str, plaintext_len);
    
    // Generate key and IV
    assert(aes_gcm_generate_key(key) == AES_GCM_SUCCESS);
    assert(aes_gcm_generate_iv(iv) == AES_GCM_SUCCESS);
    
    printf("Plaintext: %s\n", plaintext_str);
    print_hex("Key", key, AES_GCM_KEY_SIZE);
    print_hex("IV", iv, AES_GCM_IV_SIZE);
    
    // Encrypt
    int result = aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        plaintext, plaintext_len,
        NULL, 0,  // No AAD
        ciphertext,
        tag
    );
    
    assert(result == AES_GCM_SUCCESS);
    printf("✓ Encryption successful\n");
    print_hex("Ciphertext", ciphertext, plaintext_len);
    print_hex("Auth Tag", tag, AES_GCM_TAG_SIZE);
    
    // Decrypt
    result = aes_gcm_decrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        ciphertext, plaintext_len,
        NULL, 0,  // No AAD
        tag,
        decrypted
    );
    
    assert(result == AES_GCM_SUCCESS);
    printf("✓ Decryption successful\n");
    
    // Verify decrypted matches original
    assert(memcmp(plaintext, decrypted, plaintext_len) == 0);
    decrypted[plaintext_len] = '\0';  // Null-terminate for printing
    printf("Decrypted: %s\n", decrypted);
    printf("✓ Plaintext matches original\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Encryption with AAD (session_id + sequence)
void test_encryption_with_aad() {
    printf("\n=== Test 2: Encryption with AAD ===\n");
    
    uint8_t key[AES_GCM_KEY_SIZE];
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    
    const char *plaintext_str = "IP packet data for ICMP tunnel";
    size_t plaintext_len = strlen(plaintext_str);
    uint8_t plaintext[256];
    uint8_t ciphertext[256];
    uint8_t decrypted[256];
    
    // AAD: session_id (16-bit) + sequence (16-bit)
    uint16_t session_id = 0x1234;
    uint16_t sequence = 0x0042;
    uint8_t aad[4] = {
        (session_id >> 8) & 0xFF,
        session_id & 0xFF,
        (sequence >> 8) & 0xFF,
        sequence & 0xFF
    };
    
    memcpy(plaintext, plaintext_str, plaintext_len);
    
    // Generate key and IV
    aes_gcm_generate_key(key);
    aes_gcm_generate_iv(iv);
    
    printf("Session ID: 0x%04X\n", session_id);
    printf("Sequence: 0x%04X\n", sequence);
    print_hex("AAD", aad, 4);
    
    // Encrypt with AAD
    int result = aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        plaintext, plaintext_len,
        aad, 4,  // Include AAD
        ciphertext,
        tag
    );
    
    assert(result == AES_GCM_SUCCESS);
    printf("✓ Encryption with AAD successful\n");
    
    // Decrypt with correct AAD
    result = aes_gcm_decrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        ciphertext, plaintext_len,
        aad, 4,  // Same AAD
        tag,
        decrypted
    );
    
    assert(result == AES_GCM_SUCCESS);
    assert(memcmp(plaintext, decrypted, plaintext_len) == 0);
    printf("✓ Decryption with correct AAD successful\n");
    
    // Try to decrypt with wrong AAD (should fail)
    uint8_t wrong_aad[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    result = aes_gcm_decrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        ciphertext, plaintext_len,
        wrong_aad, 4,  // Wrong AAD
        tag,
        decrypted
    );
    
    assert(result == AES_GCM_ERROR_AUTH_FAILED);
    printf("✓ Decryption with wrong AAD failed as expected\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
}

// Test 3: Authentication failure with modified tag
void test_authentication_failure() {
    printf("\n=== Test 3: Authentication Failure ===\n");
    
    uint8_t key[AES_GCM_KEY_SIZE];
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    
    const char *plaintext_str = "Sensitive data";
    size_t plaintext_len = strlen(plaintext_str);
    uint8_t plaintext[256];
    uint8_t ciphertext[256];
    uint8_t decrypted[256];
    
    memcpy(plaintext, plaintext_str, plaintext_len);
    
    // Generate key and IV
    aes_gcm_generate_key(key);
    aes_gcm_generate_iv(iv);
    
    // Encrypt
    aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        plaintext, plaintext_len,
        NULL, 0,
        ciphertext,
        tag
    );
    
    printf("✓ Encryption successful\n");
    print_hex("Original Tag", tag, AES_GCM_TAG_SIZE);
    
    // Corrupt the tag
    tag[0] ^= 0xFF;
    print_hex("Corrupted Tag", tag, AES_GCM_TAG_SIZE);
    
    // Try to decrypt with corrupted tag
    int result = aes_gcm_decrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        ciphertext, plaintext_len,
        NULL, 0,
        tag,
        decrypted
    );
    
    assert(result == AES_GCM_ERROR_AUTH_FAILED);
    printf("✓ Decryption failed as expected: %s\n", aes_gcm_error_string(result));
    
    printf(COLOR_GREEN "✓✓✓ Test 3 PASSED\n" COLOR_RESET);
}

// Test 4: Large data encryption (MTU-sized packet)
void test_large_data_encryption() {
    printf("\n=== Test 4: Large Data Encryption (MTU) ===\n");
    
    uint8_t key[AES_GCM_KEY_SIZE];
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    
    // MTU size for ICMP: 1472 bytes
    const size_t data_size = 1472;
    uint8_t *plaintext = (uint8_t*)malloc(data_size);
    uint8_t *ciphertext = (uint8_t*)malloc(data_size);
    uint8_t *decrypted = (uint8_t*)malloc(data_size);
    
    if (!plaintext || !ciphertext || !decrypted) {
        printf("Memory allocation failed\n");
        free(plaintext);
        free(ciphertext);
        free(decrypted);
        return 0;
    }
    
    // Fill with test pattern
    for (size_t i = 0; i < data_size; i++) {
        plaintext[i] = i & 0xFF;
    }
    
    // Generate key and IV
    aes_gcm_generate_key(key);
    aes_gcm_generate_iv(iv);
    
    printf("Data size: %zu bytes (MTU)\n", data_size);
    
    // Encrypt
    int result = aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        plaintext, data_size,
        NULL, 0,
        ciphertext,
        tag
    );
    
    assert(result == AES_GCM_SUCCESS);
    printf("✓ Large data encryption successful\n");
    
    // Decrypt
    result = aes_gcm_decrypt(
        key, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        ciphertext, data_size,
        NULL, 0,
        tag,
        decrypted
    );
    
    assert(result == AES_GCM_SUCCESS);
    printf("✓ Large data decryption successful\n");
    
    // Verify
    assert(memcmp(plaintext, decrypted, data_size) == 0);
    printf("✓ Data integrity verified\n");
    
    // Cleanup
    free(plaintext);
    free(ciphertext);
    free(decrypted);
    
    printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
}

// Test 5: Invalid parameters
void test_invalid_parameters() {
    printf("\n=== Test 5: Invalid Parameters ===\n");
    
    uint8_t key[AES_GCM_KEY_SIZE];
    uint8_t iv[AES_GCM_IV_SIZE];
    uint8_t tag[AES_GCM_TAG_SIZE];
    uint8_t data[100];
    uint8_t output[100];
    
    // Test NULL key
    int result = aes_gcm_encrypt(
        NULL, AES_GCM_KEY_SIZE,
        iv, AES_GCM_IV_SIZE,
        data, 100,
        NULL, 0,
        output,
        tag
    );
    assert(result == AES_GCM_ERROR_INVALID_PARAM);
    printf("✓ NULL key rejected\n");
    
    // Test wrong key size
    result = aes_gcm_encrypt(
        key, 16,  // Wrong size (should be 32)
        iv, AES_GCM_IV_SIZE,
        data, 100,
        NULL, 0,
        output,
        tag
    );
    assert(result == AES_GCM_ERROR_INVALID_PARAM);
    printf("✓ Wrong key size rejected\n");
    
    // Test NULL IV
    result = aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        NULL, AES_GCM_IV_SIZE,
        data, 100,
        NULL, 0,
        output,
        tag
    );
    assert(result == AES_GCM_ERROR_INVALID_PARAM);
    printf("✓ NULL IV rejected\n");
    
    // Test wrong IV size
    result = aes_gcm_encrypt(
        key, AES_GCM_KEY_SIZE,
        iv, 16,  // Wrong size (should be 12)
        data, 100,
        NULL, 0,
        output,
        tag
    );
    assert(result == AES_GCM_ERROR_INVALID_PARAM);
    printf("✓ Wrong IV size rejected\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   AES-256-GCM Unit Tests                     ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    test_basic_encryption_decryption();
    test_encryption_with_aad();
    test_authentication_failure();
    test_large_data_encryption();
    test_invalid_parameters();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS PASSED ✓" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    return 0;
}
