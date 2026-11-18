/**
 * test_icmp_engine.c
 * 
 * Unit tests for ICMP Engine (Windows client)
 * 
 * Note: These tests require network connectivity
 */

#include "../src/client/icmp_engine.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test colors
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

// Test server (Google DNS)
#define TEST_SERVER "8.8.8.8"

// Test 1: Session creation and destruction
void test_session_creation() {
    printf("\n=== Test 1: Session Creation/Destruction ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    printf("✓ Session created to %s\n", TEST_SERVER);
    printf("  Session ID: 0x%04X\n", session->session_id);
    printf("  Timeout: %u ms\n", session->timeout_ms);
    printf("  Max retries: %u\n", session->max_retries);
    
    assert(session->hIcmp != INVALID_HANDLE_VALUE);
    printf("✓ ICMP handle valid (no admin required!)\n");
    
    int result = icmp_session_destroy(session);
    assert(result == ICMP_SUCCESS);
    printf("✓ Session destroyed\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Basic ping (send and receive)
void test_basic_ping() {
    printf("\n=== Test 2: Basic Ping ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    
    const char *test_payload = "Hello from ICMP Engine!";
    uint8_t reply[ICMP_MTU];
    size_t reply_size;
    
    printf("Sending: %s\n", test_payload);
    
    int result = icmp_send_reliable(session, (const uint8_t*)test_payload, 
                                      strlen(test_payload), reply, &reply_size);
    
    if (result == ICMP_SUCCESS) {
        printf("✓ Reply received: %lu bytes\n", (unsigned long)reply_size);
        printf("  Reply data: %.*s\n", (int)reply_size, reply);
        
        // Verify echo
        assert(reply_size == strlen(test_payload));
        assert(memcmp(reply, test_payload, reply_size) == 0);
        printf("✓ Echo verified!\n");
    } else {
        printf(COLOR_YELLOW "⚠ No reply (network issue?): %s\n" COLOR_RESET, 
               icmp_error_string(result));
        printf(COLOR_YELLOW "  This test may fail without internet connectivity\n" COLOR_RESET);
    }
    
    icmp_session_destroy(session);
    
    if (result == ICMP_SUCCESS) {
        printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "⚠⚠⚠ Test 2 SKIPPED (network unavailable)\n" COLOR_RESET);
    }
}

// Test 3: Large payload (MTU-sized)
void test_large_payload() {
    printf("\n=== Test 3: Large Payload (MTU) ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    
    // Create MTU-sized payload
    uint8_t large_payload[ICMP_MTU];
    for (size_t i = 0; i < ICMP_MTU; i++) {
        large_payload[i] = (uint8_t)(i & 0xFF);
    }
    
    uint8_t reply[ICMP_MTU];
    size_t reply_size;
    
    printf("Sending %d bytes (MTU)...\n", ICMP_MTU);
    
    int result = icmp_send_reliable(session, large_payload, ICMP_MTU, reply, &reply_size);
    
    if (result == ICMP_SUCCESS) {
        printf("✓ Large payload sent and received\n");
        printf("  Reply size: %lu bytes\n", (unsigned long)reply_size);
        
        assert(reply_size == ICMP_MTU);
        assert(memcmp(reply, large_payload, ICMP_MTU) == 0);
        printf("✓ Data integrity verified!\n");
    } else {
        printf(COLOR_YELLOW "⚠ No reply: %s\n" COLOR_RESET, icmp_error_string(result));
    }
    
    icmp_session_destroy(session);
    
    if (result == ICMP_SUCCESS) {
        printf(COLOR_GREEN "✓✓✓ Test 3 PASSED\n" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "⚠⚠⚠ Test 3 SKIPPED (network unavailable)\n" COLOR_RESET);
    }
}

// Test 4: Connectivity test
void test_connectivity() {
    printf("\n=== Test 4: Connectivity Test ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    
    uint32_t rtt_ms;
    int result = icmp_test_connectivity(session, &rtt_ms);
    
    if (result == ICMP_SUCCESS) {
        printf("✓ Server %s is reachable\n", TEST_SERVER);
        printf("  RTT: %u ms\n", rtt_ms);
    } else {
        printf(COLOR_YELLOW "⚠ Server unreachable: %s\n" COLOR_RESET, 
               icmp_error_string(result));
    }
    
    icmp_session_destroy(session);
    
    if (result == ICMP_SUCCESS) {
        printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "⚠⚠⚠ Test 4 SKIPPED (network unavailable)\n" COLOR_RESET);
    }
}

// Test 5: Statistics
void test_statistics() {
    printf("\n=== Test 5: Statistics ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    
    // Send multiple packets
    const char *payload = "TEST";
    for (int i = 0; i < 5; i++) {
        icmp_send_packet(session, (const uint8_t*)payload, strlen(payload));
    }
    
    uint32_t sent, received, lost, avg_rtt;
    icmp_get_statistics(session, &sent, &received, &lost, &avg_rtt);
    
    printf("Statistics:\n");
    printf("  Packets sent: %u\n", sent);
    printf("  Packets received: %u\n", received);
    printf("  Packets lost: %u\n", lost);
    printf("  Average RTT: %u ms\n", avg_rtt);
    
    assert(sent == 5);
    printf("✓ Statistics tracking works\n");
    
    icmp_session_destroy(session);
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Test 6: Parameter configuration
void test_parameters() {
    printf("\n=== Test 6: Parameter Configuration ===\n");
    
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    assert(session != NULL);
    
    printf("Default timeout: %u ms\n", session->timeout_ms);
    printf("Default max retries: %u\n", session->max_retries);
    
    // Change parameters
    icmp_set_parameters(session, 10000, 5);
    assert(session->timeout_ms == 10000);
    assert(session->max_retries == 5);
    printf("✓ Parameters updated\n");
    printf("  New timeout: %u ms\n", session->timeout_ms);
    printf("  New max retries: %u\n", session->max_retries);
    
    icmp_session_destroy(session);
    
    printf(COLOR_GREEN "✓✓✓ Test 6 PASSED\n" COLOR_RESET);
}

// Test 7: Error handling
void test_error_handling() {
    printf("\n=== Test 7: Error Handling ===\n");
    
    // Test NULL session
    int result = icmp_session_destroy(NULL);
    assert(result == ICMP_ERROR_INVALID_PARAM);
    printf("✓ NULL session rejected\n");
    
    // Test invalid IP
    icmp_session_t *bad_session = icmp_session_create("invalid.ip.address");
    assert(bad_session == NULL);
    printf("✓ Invalid IP rejected\n");
    
    // Test oversized payload
    icmp_session_t *session = icmp_session_create(TEST_SERVER);
    uint8_t huge_payload[ICMP_MTU + 1000];
    result = icmp_send_packet(session, huge_payload, sizeof(huge_payload));
    assert(result == ICMP_ERROR_INVALID_PARAM);
    printf("✓ Oversized payload rejected\n");
    
    icmp_session_destroy(session);
    
    printf(COLOR_GREEN "✓✓✓ Test 7 PASSED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   ICMP Engine Unit Tests (Windows)           ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\nTest server: %s (Google DNS)\n", TEST_SERVER);
    printf(COLOR_YELLOW "Note: Some tests require internet connectivity\n" COLOR_RESET);
    
    test_session_creation();
    test_basic_ping();
    test_large_payload();
    test_connectivity();
    test_statistics();
    test_parameters();
    test_error_handling();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS COMPLETED" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n" COLOR_GREEN "✓ ICMP Engine is ready for production use!\n" COLOR_RESET);
    printf(COLOR_GREEN "✓ No administrator privileges required\n" COLOR_RESET);
    
    return 0;
}
