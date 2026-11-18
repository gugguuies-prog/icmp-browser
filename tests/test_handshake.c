/**
 * test_handshake.c
 * 
 * Unit tests for DH Handshake over ICMP
 */

#include "../src/common/handshake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test colors
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

// Test 1: Initialize handshake context
void test_handshake_init() {
    printf("\n=== Test 1: Handshake Initialization ===\n");
    
    handshake_context_t ctx;
    int result = handshake_init(&ctx, 0x1234);
    
    assert(result == HANDSHAKE_SUCCESS);
    printf("✓ Handshake context initialized\n");
    printf("  Session ID: 0x%04X\n", ctx.session_id);
    printf("  State: %s\n", handshake_state_string(ctx.state));
    printf("  Timeout: %u ms\n", ctx.timeout_ms);
    printf("  Max retries: %u\n", ctx.max_retries);
    
    assert(ctx.state == HANDSHAKE_STATE_INIT);
    assert(ctx.session_id == 0x1234);
    assert(ctx.timeout_ms == HANDSHAKE_TIMEOUT_MS);
    
    handshake_cleanup(&ctx);
    printf("✓ Handshake context cleaned up\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Client start handshake
void test_client_start() {
    printf("\n=== Test 2: Client Start Handshake ===\n");
    
    handshake_context_t ctx;
    handshake_init(&ctx, 0x5678);
    
    proto_packet_t request;
    int result = handshake_client_start(&ctx, &request);
    
    assert(result == HANDSHAKE_SUCCESS);
    printf("✓ Client started handshake\n");
    printf("  State: %s\n", handshake_state_string(ctx.state));
    printf("  Request packet type: %s\n", proto_type_string(request.header.type));
    printf("  Request payload size: %zu bytes\n", request.payload_size);
    
    assert(ctx.state == HANDSHAKE_STATE_SENT_REQUEST);
    assert(request.header.type == PROTO_TYPE_HANDSHAKE);
    assert(request.payload_size == 256);
    
    // Verify client public key is not all zeros
    int non_zero = 0;
    for (int i = 0; i < 256; i++) {
        if (ctx.client_public_key[i] != 0) {
            non_zero = 1;
            break;
        }
    }
    assert(non_zero);
    printf("✓ Client public key generated (non-zero)\n");
    
    handshake_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
}

// Test 3: Complete client-server handshake
void test_complete_handshake() {
    printf("\n=== Test 3: Complete Client-Server Handshake ===\n");
    
    // CLIENT: Initialize and start
    handshake_context_t client_ctx;
    handshake_init(&client_ctx, 0xABCD);
    
    proto_packet_t request;
    int result = handshake_client_start(&client_ctx, &request);
    assert(result == HANDSHAKE_SUCCESS);
    printf("✓ Client: Handshake started\n");
    printf("  Client state: %s\n", handshake_state_string(client_ctx.state));
    
    // SERVER: Process request and generate response
    handshake_context_t server_ctx;
    handshake_init(&server_ctx, 0xABCD);
    
    proto_packet_t response;
    result = handshake_server_process(&server_ctx, &request, &response);
    assert(result == HANDSHAKE_SUCCESS);
    printf("✓ Server: Request processed\n");
    printf("  Server state: %s\n", handshake_state_string(server_ctx.state));
    printf("  Response packet type: %s\n", proto_type_string(response.header.type));
    
    assert(server_ctx.state == HANDSHAKE_STATE_COMPLETED);
    assert(response.header.type == PROTO_TYPE_HANDSHAKE);
    
    // CLIENT: Finish handshake
    result = handshake_client_finish(&client_ctx, &response);
    assert(result == HANDSHAKE_SUCCESS);
    printf("✓ Client: Handshake completed\n");
    printf("  Client state: %s\n", handshake_state_string(client_ctx.state));
    
    assert(client_ctx.state == HANDSHAKE_STATE_COMPLETED);
    assert(handshake_is_complete(&client_ctx));
    assert(handshake_is_complete(&server_ctx));
    
    // Verify both sides have the same session key
    uint8_t client_key[32], server_key[32];
    handshake_get_session_key(&client_ctx, client_key);
    handshake_get_session_key(&server_ctx, server_key);
    
    printf("✓ Session keys derived\n");
    printf("  Client key (first 8 bytes): ");
    for (int i = 0; i < 8; i++) {
        printf("%02X ", client_key[i]);
    }
    printf("\n");
    printf("  Server key (first 8 bytes): ");
    for (int i = 0; i < 8; i++) {
        printf("%02X ", server_key[i]);
    }
    printf("\n");
    
    assert(memcmp(client_key, server_key, 32) == 0);
    printf("✓ Session keys MATCH! (%s)\n", 
           memcmp(client_key, server_key, 32) == 0 ? "IDENTICAL" : "DIFFERENT");
    
    // Verify keys are not all zeros
    int non_zero = 0;
    for (int i = 0; i < 32; i++) {
        if (client_key[i] != 0) {
            non_zero = 1;
            break;
        }
    }
    assert(non_zero);
    printf("✓ Session key is valid (non-zero)\n");
    
    handshake_cleanup(&client_ctx);
    handshake_cleanup(&server_ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 3 PASSED - HANDSHAKE SUCCESSFUL!\n" COLOR_RESET);
}

// Test 4: Multiple handshakes with different session IDs
void test_multiple_handshakes() {
    printf("\n=== Test 4: Multiple Handshakes ===\n");
    
    for (int i = 0; i < 3; i++) {
        uint16_t session_id = 0x1000 + i;
        printf("\nHandshake %d (Session ID: 0x%04X):\n", i + 1, session_id);
        
        // Client
        handshake_context_t client_ctx;
        handshake_init(&client_ctx, session_id);
        proto_packet_t request;
        handshake_client_start(&client_ctx, &request);
        
        // Server
        handshake_context_t server_ctx;
        handshake_init(&server_ctx, session_id);
        proto_packet_t response;
        handshake_server_process(&server_ctx, &request, &response);
        
        // Client finish
        handshake_client_finish(&client_ctx, &response);
        
        // Verify
        uint8_t client_key[32], server_key[32];
        handshake_get_session_key(&client_ctx, client_key);
        handshake_get_session_key(&server_ctx, server_key);
        
        assert(memcmp(client_key, server_key, 32) == 0);
        printf("  ✓ Keys match\n");
        
        handshake_cleanup(&client_ctx);
        handshake_cleanup(&server_ctx);
    }
    
    printf("\n✓ All handshakes successful\n");
    printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
}

// Test 5: Handshake timeout configuration
void test_timeout_config() {
    printf("\n=== Test 5: Timeout Configuration ===\n");
    
    handshake_context_t ctx;
    handshake_init(&ctx, 0xDEAD);
    
    printf("Default timeout: %u ms\n", ctx.timeout_ms);
    printf("Default max retries: %u\n", ctx.max_retries);
    
    // Change timeout
    int result = handshake_set_timeout(&ctx, 10000, 5);
    assert(result == HANDSHAKE_SUCCESS);
    
    printf("New timeout: %u ms\n", ctx.timeout_ms);
    printf("New max retries: %u\n", ctx.max_retries);
    
    assert(ctx.timeout_ms == 10000);
    assert(ctx.max_retries == 5);
    printf("✓ Timeout configuration updated\n");
    
    handshake_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Test 6: Error handling
void test_error_handling() {
    printf("\n=== Test 6: Error Handling ===\n");
    
    // Test NULL parameters
    int result = handshake_init(NULL, 0);
    assert(result == HANDSHAKE_ERROR_INVALID_PARAM);
    printf("✓ NULL context rejected\n");
    
    handshake_context_t ctx;
    handshake_init(&ctx, 0xBEEF);
    
    // Test invalid state transition
    proto_packet_t response;
    result = handshake_client_finish(&ctx, &response);
    assert(result == HANDSHAKE_ERROR_PROTOCOL);
    printf("✓ Invalid state transition rejected\n");
    
    // Test get key before completion
    uint8_t key[32];
    result = handshake_get_session_key(&ctx, key);
    assert(result == HANDSHAKE_ERROR_PROTOCOL);
    printf("✓ Get key before completion rejected\n");
    
    handshake_cleanup(&ctx);
    
    printf(COLOR_GREEN "✓✓✓ Test 6 PASSED\n" COLOR_RESET);
}

// Test 7: Key uniqueness
void test_key_uniqueness() {
    printf("\n=== Test 7: Session Key Uniqueness ===\n");
    
    uint8_t keys[3][32];
    
    // Generate 3 handshakes
    for (int i = 0; i < 3; i++) {
        handshake_context_t client_ctx, server_ctx;
        handshake_init(&client_ctx, 0x2000 + i);
        handshake_init(&server_ctx, 0x2000 + i);
        
        proto_packet_t request, response;
        handshake_client_start(&client_ctx, &request);
        handshake_server_process(&server_ctx, &request, &response);
        handshake_client_finish(&client_ctx, &response);
        
        handshake_get_session_key(&client_ctx, keys[i]);
        
        handshake_cleanup(&client_ctx);
        handshake_cleanup(&server_ctx);
    }
    
    // Verify all keys are different
    assert(memcmp(keys[0], keys[1], 32) != 0);
    assert(memcmp(keys[1], keys[2], 32) != 0);
    assert(memcmp(keys[0], keys[2], 32) != 0);
    
    printf("✓ All session keys are unique\n");
    printf("  Key 1 (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02X ", keys[0][i]);
    printf("\n");
    printf("  Key 2 (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02X ", keys[1][i]);
    printf("\n");
    printf("  Key 3 (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02X ", keys[2][i]);
    printf("\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 7 PASSED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   DH Handshake over ICMP Unit Tests         ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    test_handshake_init();
    test_client_start();
    test_complete_handshake();
    test_multiple_handshakes();
    test_timeout_config();
    test_error_handling();
    test_key_uniqueness();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS COMPLETED" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n" COLOR_GREEN "✓ DH Handshake implementation is ready!\n" COLOR_RESET);
    printf("  - Client-server key exchange functional\n");
    printf("  - Session keys derived correctly (AES-256)\n");
    printf("  - Both sides get identical keys\n");
    printf("  - Keys are unique per handshake\n");
    printf("  - Ready for ICMP tunnel encryption\n");
    
    return 0;
}
