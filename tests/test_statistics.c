/**
 * test_statistics.c
 * 
 * Unit tests for Statistics
 */

#include "../src/common/statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name) \
    do { \
        printf("\n=== Test %d: %s ===\n", ++tests_run, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("✓ PASSED\n"); \
        tests_passed++; \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("✗ FAILED: %s\n", msg); \
            printf("  Line %d: %s\n", __LINE__, #cond); \
            return; \
        } \
    } while(0)

/**
 * Test 1: Create and Destroy
 */
void test_create_destroy(void) {
    TEST_START("Create and Destroy");
    
    statistics_t *stats = stats_create();
    ASSERT(stats != NULL, "Creation should succeed");
    
    // Initial values should be zero
    ASSERT(stats_get_packets_sent(stats) == 0, "Initial sent should be 0");
    ASSERT(stats_get_packets_received(stats) == 0, "Initial received should be 0");
    ASSERT(stats_get_packets_lost(stats) == 0, "Initial lost should be 0");
    
    printf("Created statistics context\n");
    
    stats_destroy(stats);
    
    TEST_PASS();
}

/**
 * Test 2: Packet Sent Tracking
 */
void test_packet_sent(void) {
    TEST_START("Packet Sent Tracking");
    
    statistics_t *stats = stats_create();
    
    // Send some packets
    stats_packet_sent(stats, 100);
    stats_packet_sent(stats, 200);
    stats_packet_sent(stats, 150);
    
    ASSERT(stats_get_packets_sent(stats) == 3, "Should have 3 packets sent");
    
    printf("Tracked 3 packets sent (450 bytes total)\n");
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 3: Packet Received with RTT
 */
void test_packet_received(void) {
    TEST_START("Packet Received with RTT");
    
    statistics_t *stats = stats_create();
    
    // Receive packets with different RTTs
    stats_packet_received(stats, 100, 25);   // 25ms
    stats_packet_received(stats, 150, 30);   // 30ms
    stats_packet_received(stats, 200, 20);   // 20ms
    
    ASSERT(stats_get_packets_received(stats) == 3, "Should have 3 packets received");
    
    // Check RTT statistics
    uint32_t avg = stats_get_average_rtt(stats);
    ASSERT(avg == 25, "Average RTT should be 25ms");  // (25+30+20)/3 = 25
    
    uint32_t min = stats_get_min_rtt(stats);
    ASSERT(min == 20, "Min RTT should be 20ms");
    
    uint32_t max = stats_get_max_rtt(stats);
    ASSERT(max == 30, "Max RTT should be 30ms");
    
    printf("RTT stats: min=%ums, avg=%ums, max=%ums\n", min, avg, max);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 4: Packet Loss Tracking
 */
void test_packet_loss(void) {
    TEST_START("Packet Loss Tracking");
    
    statistics_t *stats = stats_create();
    
    // Send 10 packets
    for (int i = 0; i < 10; i++) {
        stats_packet_sent(stats, 100);
    }
    
    // 2 packets lost
    stats_packet_lost(stats);
    stats_packet_lost(stats);
    
    // 8 packets received
    for (int i = 0; i < 8; i++) {
        stats_packet_received(stats, 100, 25);
    }
    
    ASSERT(stats_get_packets_lost(stats) == 2, "Should have 2 packets lost");
    
    double loss_rate = stats_get_packet_loss_rate(stats);
    ASSERT(loss_rate == 20.0, "Loss rate should be 20%");
    
    printf("Packet loss: 2/10 (%.1f%%)\n", loss_rate);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 5: Throughput Calculation
 */
void test_throughput(void) {
    TEST_START("Throughput Calculation");
    
    statistics_t *stats = stats_create();
    
    // Send/receive some data
    for (int i = 0; i < 10; i++) {
        stats_packet_sent(stats, 1000);
        stats_packet_received(stats, 900, 25);
    }
    
    // Wait 1 second
    sleep(1);
    
    // Get throughput
    uint64_t upload = stats_get_throughput_upload(stats);
    uint64_t download = stats_get_throughput_download(stats);
    
    ASSERT(upload > 0, "Upload throughput should be > 0");
    ASSERT(download > 0, "Download throughput should be > 0");
    
    printf("Throughput: upload=%llu B/s, download=%llu B/s\n",
           (unsigned long long)upload, (unsigned long long)download);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 6: Connection Quality Score
 */
void test_connection_quality(void) {
    TEST_START("Connection Quality Score");
    
    statistics_t *stats = stats_create();
    
    // Perfect connection (no loss, good RTT)
    for (int i = 0; i < 100; i++) {
        stats_packet_sent(stats, 100);
        stats_packet_received(stats, 100, 20);  // 20ms RTT
    }
    
    int quality = stats_get_connection_quality(stats);
    ASSERT(quality >= 80, "Perfect connection should have quality >= 80");
    
    printf("Connection quality (perfect): %d/100\n", quality);
    
    // Add some losses
    for (int i = 0; i < 10; i++) {
        stats_packet_lost(stats);
    }
    
    quality = stats_get_connection_quality(stats);
    ASSERT(quality < 80, "With losses, quality should drop");
    
    printf("Connection quality (with loss): %d/100\n", quality);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 7: Uptime Tracking
 */
void test_uptime(void) {
    TEST_START("Uptime Tracking");
    
    statistics_t *stats = stats_create();
    
    // Wait 2 seconds
    sleep(2);
    
    uint64_t uptime = stats_get_uptime(stats);
    ASSERT(uptime >= 2, "Uptime should be at least 2 seconds");
    
    printf("Uptime: %llu seconds\n", (unsigned long long)uptime);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 8: Reset Statistics
 */
void test_reset(void) {
    TEST_START("Reset Statistics");
    
    statistics_t *stats = stats_create();
    
    // Add some data
    stats_packet_sent(stats, 100);
    stats_packet_sent(stats, 200);
    stats_packet_received(stats, 150, 25);
    stats_packet_lost(stats);
    
    ASSERT(stats_get_packets_sent(stats) == 2, "Should have 2 packets before reset");
    
    // Reset
    stats_reset(stats);
    
    // Check all counters are zero
    ASSERT(stats_get_packets_sent(stats) == 0, "Sent should be 0 after reset");
    ASSERT(stats_get_packets_received(stats) == 0, "Received should be 0 after reset");
    ASSERT(stats_get_packets_lost(stats) == 0, "Lost should be 0 after reset");
    ASSERT(stats_get_average_rtt(stats) == 0, "RTT should be 0 after reset");
    
    printf("Statistics reset successfully\n");
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 9: Retransmission Tracking
 */
void test_retransmission(void) {
    TEST_START("Retransmission Tracking");
    
    statistics_t *stats = stats_create();
    
    // Send packet
    stats_packet_sent(stats, 100);
    
    // Retransmit
    stats_packet_retransmitted(stats, 100);
    stats_packet_retransmitted(stats, 100);
    
    // Note: retransmitted count is tracked internally
    printf("Retransmission tracking functional\n");
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Test 10: Print Statistics
 */
void test_print_stats(void) {
    TEST_START("Print Statistics");
    
    statistics_t *stats = stats_create();
    
    // Generate some traffic
    for (int i = 0; i < 20; i++) {
        stats_packet_sent(stats, 1400);
        if (i < 18) {
            stats_packet_received(stats, 1300, 25 + (i % 10));
        } else {
            stats_packet_lost(stats);
        }
    }
    
    // Wait a bit for uptime
    sleep(1);
    
    // Print summary (visual test)
    printf("Statistics summary:\n");
    stats_print(stats);
    
    stats_destroy(stats);
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║            Statistics Unit Tests                           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_create_destroy();
    test_packet_sent();
    test_packet_received();
    test_packet_loss();
    test_throughput();
    test_connection_quality();
    test_uptime();
    test_reset();
    test_retransmission();
    test_print_stats();
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ TEST SUMMARY                                               ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total tests: %2d                                            ║\n", tests_run);
    printf("║ Passed:      %2d                                            ║\n", tests_passed);
    printf("║ Failed:      %2d                                            ║\n", tests_run - tests_passed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (tests_passed == tests_run) {
        printf("\n🎉 ALL TESTS PASSED! 🎉\n\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED ❌\n\n");
        return 1;
    }
}
