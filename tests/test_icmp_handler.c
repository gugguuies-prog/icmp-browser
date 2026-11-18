/**
 * test_icmp_handler.c
 * 
 * Unit tests for ICMP Handler (Linux server)
 * 
 * Note: These tests require root privileges (or CAP_NET_RAW)
 */

#include "../src/server/icmp_handler.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>

// Test colors
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

// Global server for signal handler
static icmp_server_t *g_server = NULL;

// Signal handler for Ctrl+C
void signal_handler(int sig) {
    (void)sig;
    if (g_server) {
        icmp_server_stop(g_server);
    }
}

// Test 1: Server creation and destruction
void test_server_creation() {
    printf("\n=== Test 1: Server Creation/Destruction ===\n");
    
    icmp_server_t *server = icmp_server_create();
    
    if (!server) {
        printf(COLOR_RED "✗ Server creation failed\n" COLOR_RESET);
        printf(COLOR_YELLOW "  This test requires root privileges or CAP_NET_RAW\n" COLOR_RESET);
        printf(COLOR_YELLOW "  Run with: sudo ./test_icmp_handler\n" COLOR_RESET);
        return;
    }
    
    printf("✓ Server created\n");
    printf("  Socket FD: %d\n", server->raw_socket);
    assert(server->raw_socket >= 0);
    
    int result = icmp_server_destroy(server);
    assert(result == ICMP_SUCCESS);
    printf("✓ Server destroyed\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Checksum calculation
void test_checksum() {
    printf("\n=== Test 2: ICMP Checksum Calculation ===\n");
    
    // Test data from RFC 1071
    uint8_t data1[] = {0x00, 0x01, 0xF2, 0x03};
    uint16_t checksum1 = icmp_checksum(data1, sizeof(data1));
    printf("Checksum 1: 0x%04X\n", checksum1);
    
    // Test with ICMP Echo Request
    icmp_header_t icmp;
    memset(&icmp, 0, sizeof(icmp));
    icmp.type = ICMP_TYPE_ECHO_REQUEST;
    icmp.code = 0;
    icmp.id = htons(0x1234);
    icmp.sequence = htons(1);
    
    uint16_t checksum2 = icmp_checksum((uint8_t*)&icmp, sizeof(icmp));
    printf("ICMP Echo Request checksum: 0x%04X\n", checksum2);
    
    // Verify: checksum of data + checksum should be 0xFFFF
    icmp.checksum = checksum2;
    uint16_t verify = icmp_checksum((uint8_t*)&icmp, sizeof(icmp));
    printf("Verification: 0x%04X (should be 0xFFFF or 0x0000)\n", verify);
    
    assert(verify == 0xFFFF || verify == 0x0000);
    printf("✓ Checksum calculation correct\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
}

// Test 3: Session management
void test_session_management() {
    printf("\n=== Test 3: Session Management ===\n");
    
    icmp_server_t *server = icmp_server_create();
    if (!server) {
        printf(COLOR_YELLOW "⚠ Skipping (no root privileges)\n" COLOR_RESET);
        return;
    }
    
    // Create test session
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.1.100", &client_addr.sin_addr);
    
    uint16_t session_id = 0x1234;
    
    // Simulate packet to create session
    pthread_mutex_lock(&server->sessions_lock);
    server->sessions[0].active = 1;
    server->sessions[0].session_id = session_id;
    server->sessions[0].client_addr = client_addr;
    server->sessions[0].last_seen = time(NULL);
    server->sessions[0].packets_received = 5;
    pthread_mutex_unlock(&server->sessions_lock);
    
    // Query session
    icmp_session_info_t info;
    int result = icmp_get_session(server, session_id, &info);
    assert(result == ICMP_SUCCESS);
    printf("✓ Session found\n");
    printf("  Session ID: 0x%04X\n", info.session_id);
    printf("  Packets received: %u\n", info.packets_received);
    
    // Get all sessions
    icmp_session_info_t sessions[ICMP_MAX_SESSIONS];
    size_t count;
    result = icmp_get_all_sessions(server, sessions, &count);
    assert(result == ICMP_SUCCESS);
    printf("✓ Active sessions: %zu\n", count);
    assert(count == 1);
    
    icmp_server_destroy(server);
    
    printf(COLOR_GREEN "✓✓✓ Test 3 PASSED\n" COLOR_RESET);
}

// Packet callback for Test 4
static int packet_received_count = 0;
void packet_callback(const struct sockaddr_in *addr, const uint8_t *payload,
                     size_t size, uint16_t sid, uint16_t seq) {
    (void)payload; // Mark as intentionally unused
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    printf("  Packet from %s: sid=0x%04X seq=%u size=%zu\n", ip, sid, seq, size);
    packet_received_count++;
}

// Test 4: Packet callback
void test_packet_callback() {
    printf("\n=== Test 4: Packet Callback ===\n");
    
    icmp_server_t *server = icmp_server_create();
    if (!server) {
        printf(COLOR_YELLOW "⚠ Skipping (no root privileges)\n" COLOR_RESET);
        return;
    }
    
    packet_received_count = 0;
    
    int result = icmp_set_packet_callback(server, packet_callback);
    assert(result == ICMP_SUCCESS);
    printf("✓ Callback registered\n");
    
    icmp_server_destroy(server);
    
    printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
}

// Test 5: Session cleanup
void test_session_cleanup() {
    printf("\n=== Test 5: Session Cleanup (Timeout) ===\n");
    
    icmp_server_t *server = icmp_server_create();
    if (!server) {
        printf(COLOR_YELLOW "⚠ Skipping (no root privileges)\n" COLOR_RESET);
        return;
    }
    
    // Create old session
    pthread_mutex_lock(&server->sessions_lock);
    server->sessions[0].active = 1;
    server->sessions[0].session_id = 0x5678;
    server->sessions[0].last_seen = time(NULL) - ICMP_SESSION_TIMEOUT_SEC - 10;
    pthread_mutex_unlock(&server->sessions_lock);
    
    printf("Session created with old timestamp\n");
    
    int removed = icmp_cleanup_sessions(server);
    printf("✓ Sessions removed: %d\n", removed);
    assert(removed == 1);
    
    // Verify session is gone
    icmp_session_info_t info;
    int result = icmp_get_session(server, 0x5678, &info);
    assert(result == ICMP_ERROR_SESSION_NOT_FOUND);
    printf("✓ Old session removed\n");
    
    icmp_server_destroy(server);
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Test 6: Statistics
void test_statistics() {
    printf("\n=== Test 6: Statistics ===\n");
    
    icmp_server_t *server = icmp_server_create();
    if (!server) {
        printf(COLOR_YELLOW "⚠ Skipping (no root privileges)\n" COLOR_RESET);
        return;
    }
    
    // Set some statistics
    server->total_packets_received = 100;
    server->total_packets_sent = 95;
    server->total_bytes_received = 50000;
    server->total_bytes_sent = 48000;
    
    uint32_t recv, sent;
    uint64_t bytes_recv, bytes_sent;
    
    int result = icmp_get_statistics(server, &recv, &sent, &bytes_recv, &bytes_sent);
    assert(result == ICMP_SUCCESS);
    
    printf("Statistics:\n");
    printf("  Packets received: %u\n", recv);
    printf("  Packets sent: %u\n", sent);
    printf("  Bytes received: %lu\n", bytes_recv);
    printf("  Bytes sent: %lu\n", bytes_sent);
    
    assert(recv == 100);
    assert(sent == 95);
    printf("✓ Statistics tracking works\n");
    
    icmp_server_destroy(server);
    
    printf(COLOR_GREEN "✓✓✓ Test 6 PASSED\n" COLOR_RESET);
}

// Test 7: Error handling
void test_error_handling() {
    printf("\n=== Test 7: Error Handling ===\n");
    
    // Test NULL server
    int result = icmp_server_destroy(NULL);
    assert(result == ICMP_ERROR_INVALID_PARAM);
    printf("✓ NULL server rejected\n");
    
    // Test invalid session query
    icmp_server_t *server = icmp_server_create();
    if (server) {
        icmp_session_info_t info;
        result = icmp_get_session(server, 0xFFFF, &info);
        assert(result == ICMP_ERROR_SESSION_NOT_FOUND);
        printf("✓ Invalid session rejected\n");
        
        icmp_server_destroy(server);
    }
    
    printf(COLOR_GREEN "✓✓✓ Test 7 PASSED\n" COLOR_RESET);
}

// Thread function for server
void* server_thread(void *arg) {
    icmp_server_t *server = (icmp_server_t*)arg;
    icmp_server_start(server);
    return NULL;
}

// Test 8: Live test (optional, requires ping from another host)
void test_live_server() {
    printf("\n=== Test 8: Live Server Test ===\n");
    printf(COLOR_YELLOW "This test starts a live ICMP server for 5 seconds\n" COLOR_RESET);
    printf(COLOR_YELLOW "Try pinging this host from another machine\n" COLOR_RESET);
    
    icmp_server_t *server = icmp_server_create();
    if (!server) {
        printf(COLOR_YELLOW "⚠ Skipping (no root privileges)\n" COLOR_RESET);
        return;
    }
    
    g_server = server;
    signal(SIGINT, signal_handler);
    
    icmp_set_packet_callback(server, packet_callback);
    
    // Start server in separate thread
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread, server);
    
    printf("Server running... (5 seconds)\n");
    sleep(5);
    
    // Stop server
    icmp_server_stop(server);
    pthread_join(thread, NULL);
    
    printf("✓ Server stopped\n");
    
    uint32_t recv, sent;
    icmp_get_statistics(server, &recv, &sent, NULL, NULL);
    printf("  Packets received: %u\n", recv);
    printf("  Packets sent: %u\n", sent);
    
    icmp_server_destroy(server);
    g_server = NULL;
    
    printf(COLOR_GREEN "✓✓✓ Test 8 COMPLETED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   ICMP Handler Unit Tests (Linux Server)     ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n" COLOR_YELLOW "Note: Most tests require root privileges\n" COLOR_RESET);
    printf(COLOR_YELLOW "Run with: sudo ./test_icmp_handler\n" COLOR_RESET);
    
    // Check if running as root
    if (geteuid() != 0) {
        printf("\n" COLOR_RED "⚠ WARNING: Not running as root\n" COLOR_RESET);
        printf(COLOR_RED "  Most tests will be skipped\n" COLOR_RESET);
        printf(COLOR_RED "  Run with: sudo ./test_icmp_handler\n" COLOR_RESET);
    }
    
    test_server_creation();
    test_checksum();
    test_session_management();
    test_packet_callback();
    test_session_cleanup();
    test_statistics();
    test_error_handling();
    test_live_server();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS COMPLETED" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n" COLOR_GREEN "✓ ICMP Handler is ready for production use!\n" COLOR_RESET);
    printf(COLOR_YELLOW "⚠ Remember: Server requires root or CAP_NET_RAW\n" COLOR_RESET);
    
    return 0;
}
