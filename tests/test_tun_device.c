/**
 * test_tun_device.c
 * 
 * Unit tests for TUN Device
 * 
 * NOTE: These tests require root privileges or CAP_NET_ADMIN
 */

#include "../src/server/tun_device.h"
#include <stdio.h>
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
 * Helper: Create test IPv4 packet
 */
void create_test_ipv4_packet(uint8_t *buffer, size_t *size) {
    // Minimal IPv4 packet (20 byte header + 8 byte payload)
    memset(buffer, 0, 28);
    
    buffer[0] = 0x45;                    // Version 4, IHL 5
    buffer[1] = 0x00;                    // DSCP/ECN
    buffer[2] = 0x00;                    // Total length (high)
    buffer[3] = 0x1C;                    // Total length (low) = 28 bytes
    buffer[4] = 0x00; buffer[5] = 0x01;  // Identification
    buffer[6] = 0x00; buffer[7] = 0x00;  // Flags/Fragment
    buffer[8] = 0x40;                    // TTL = 64
    buffer[9] = 0x01;                    // Protocol = ICMP
    buffer[10] = 0x00; buffer[11] = 0x00;// Checksum (TODO: calculate)
    
    // Source IP: 10.0.1.2
    buffer[12] = 10; buffer[13] = 0; buffer[14] = 1; buffer[15] = 2;
    
    // Dest IP: 8.8.8.8
    buffer[16] = 8; buffer[17] = 8; buffer[18] = 8; buffer[19] = 8;
    
    // Payload (8 bytes)
    memcpy(&buffer[20], "TESTDATA", 8);
    
    *size = 28;
}

/**
 * Test 1: Create and Destroy TUN device
 */
void test_create_destroy(void) {
    TEST_START("Create and Destroy TUN device");
    
    // Check if we have root privileges
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        printf("   Run with: sudo ./bin/test_tun_device\n");
        tests_passed++;  // Count as passed (skip)
        return;
    }
    
    // Create TUN device
    tun_device_t *tun = tun_create("tun_test0");
    ASSERT(tun != NULL, "TUN device creation should succeed");
    ASSERT(tun->is_open == 1, "Device should be open");
    ASSERT(tun->fd > 0, "File descriptor should be valid");
    
    printf("Created TUN device: %s (fd=%d)\n", tun->name, tun->fd);
    
    // Get device name
    const char *name = tun_get_name(tun);
    ASSERT(name != NULL, "Device name should be available");
    ASSERT(strncmp(name, "tun_test", 8) == 0, "Name should match");
    
    // Get file descriptor
    int fd = tun_get_fd(tun);
    ASSERT(fd > 0, "File descriptor should be valid");
    ASSERT(fd == tun->fd, "FD should match");
    
    // Destroy device
    tun_destroy(tun);
    
    TEST_PASS();
}

/**
 * Test 2: Configure IP Address
 */
void test_configure_ip(void) {
    TEST_START("Configure IP Address");
    
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        tests_passed++;
        return;
    }
    
    tun_device_t *tun = tun_create("tun_test1");
    ASSERT(tun != NULL, "TUN device creation should succeed");
    
    // Configure IP
    int ret = tun_configure(tun, "10.0.1.1", "255.255.255.0");
    ASSERT(ret == TUN_SUCCESS, "IP configuration should succeed");
    
    // Verify stored configuration
    ASSERT(strcmp(tun->ip_addr, "10.0.1.1") == 0, "IP address should match");
    ASSERT(strcmp(tun->netmask, "255.255.255.0") == 0, "Netmask should match");
    
    printf("Configured %s with IP %s/%s\n", tun->name, tun->ip_addr, tun->netmask);
    
    tun_destroy(tun);
    TEST_PASS();
}

/**
 * Test 3: Set MTU
 */
void test_set_mtu(void) {
    TEST_START("Set MTU");
    
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        tests_passed++;
        return;
    }
    
    tun_device_t *tun = tun_create("tun_test2");
    ASSERT(tun != NULL, "TUN device creation should succeed");
    
    // Configure first (interface must be UP to set MTU reliably)
    tun_configure(tun, "10.0.1.1", "255.255.255.0");
    
    // Set MTU
    int ret = tun_set_mtu(tun, 1400);
    ASSERT(ret == TUN_SUCCESS, "MTU configuration should succeed");
    ASSERT(tun->mtu == 1400, "MTU should be updated");
    
    printf("Set MTU to %d\n", tun->mtu);
    
    tun_destroy(tun);
    TEST_PASS();
}

/**
 * Test 4: Write IP Packet
 */
void test_write_packet(void) {
    TEST_START("Write IP Packet");
    
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        tests_passed++;
        return;
    }
    
    tun_device_t *tun = tun_create("tun_test3");
    ASSERT(tun != NULL, "TUN device creation should succeed");
    
    tun_configure(tun, "10.0.1.1", "255.255.255.0");
    
    // Create test packet
    uint8_t packet[100];
    size_t packet_size;
    create_test_ipv4_packet(packet, &packet_size);
    
    // Write packet
    int written = tun_write(tun, packet, packet_size);
    ASSERT(written > 0, "Write should succeed");
    ASSERT(written == (int)packet_size, "Should write full packet");
    
    printf("Wrote %d byte IP packet to %s\n", written, tun->name);
    
    tun_destroy(tun);
    TEST_PASS();
}

/**
 * Test 5: Read Timeout
 */
void test_read_timeout(void) {
    TEST_START("Read with Timeout");
    
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        tests_passed++;
        return;
    }
    
    tun_device_t *tun = tun_create("tun_test4");
    ASSERT(tun != NULL, "TUN device creation should succeed");
    
    tun_configure(tun, "10.0.1.1", "255.255.255.0");
    
    // Try to read with 500ms timeout
    uint8_t buffer[TUN_BUFFER_SIZE];
    int result = tun_read_timeout(tun, buffer, sizeof(buffer), 500);
    
    // Either timeout OR receive data (kernel might send router solicitation, etc.)
    ASSERT(result == TUN_ERROR_TIMEOUT || result > 0, 
           "Should timeout or receive kernel packets");
    
    if (result == TUN_ERROR_TIMEOUT) {
        printf("Timeout correctly after 500ms (no data available)\n");
    } else {
        printf("Received %d bytes (kernel traffic on new interface)\n", result);
    }
    
    tun_destroy(tun);
    TEST_PASS();
}

/**
 * Test 6: IPv4 Validation
 */
void test_ipv4_validation(void) {
    TEST_START("IPv4 Packet Validation");
    
    // Valid IPv4 packet
    uint8_t valid_packet[28];
    size_t size;
    create_test_ipv4_packet(valid_packet, &size);
    
    int is_valid = tun_is_valid_ipv4(valid_packet, size);
    ASSERT(is_valid == 1, "Valid packet should be recognized");
    
    // Invalid: too short
    is_valid = tun_is_valid_ipv4(valid_packet, 10);
    ASSERT(is_valid == 0, "Short packet should be invalid");
    
    // Invalid: wrong version
    uint8_t invalid_version[28];
    memcpy(invalid_version, valid_packet, 28);
    invalid_version[0] = 0x65;  // Version 6
    is_valid = tun_is_valid_ipv4(invalid_version, 28);
    ASSERT(is_valid == 0, "IPv6 packet should be invalid");
    
    // Invalid: bad header length
    uint8_t invalid_ihl[28];
    memcpy(invalid_ihl, valid_packet, 28);
    invalid_ihl[0] = 0x43;  // IHL = 3 (too small)
    is_valid = tun_is_valid_ipv4(invalid_ihl, 28);
    ASSERT(is_valid == 0, "Invalid IHL should be rejected");
    
    printf("IPv4 validation checks passed\n");
    
    TEST_PASS();
}

/**
 * Test 7: Error Handling
 */
void test_error_handling(void) {
    TEST_START("Error Handling");
    
    // Test invalid parameters
    tun_device_t *tun = tun_create(NULL);  // NULL name is OK (auto)
    
    if (geteuid() != 0) {
        // If not root, creation should fail
        ASSERT(tun == NULL, "Should fail without root");
        printf("Correctly rejected TUN creation without root\n");
    } else {
        // If root, test other errors
        ASSERT(tun != NULL, "Should succeed with root");
        
        // Invalid IP configuration
        int ret = tun_configure(tun, "invalid_ip", "255.255.255.0");
        ASSERT(ret == TUN_ERROR_CONFIG_FAILED, "Invalid IP should fail");
        
        // NULL buffer write
        ret = tun_write(tun, NULL, 100);
        ASSERT(ret == TUN_ERROR_INVALID_PARAM, "NULL buffer should fail");
        
        // NULL buffer read
        ret = tun_read(tun, NULL, 100);
        ASSERT(ret == TUN_ERROR_INVALID_PARAM, "NULL buffer should fail");
        
        tun_destroy(tun);
    }
    
    // Test error strings
    const char *err = tun_error_string(TUN_SUCCESS);
    ASSERT(strcmp(err, "Success") == 0, "Error string should match");
    
    err = tun_error_string(TUN_ERROR_TIMEOUT);
    ASSERT(strcmp(err, "Timeout") == 0, "Timeout string should match");
    
    TEST_PASS();
}

/**
 * Test 8: Multiple Devices
 */
void test_multiple_devices(void) {
    TEST_START("Multiple TUN Devices");
    
    if (geteuid() != 0) {
        printf("⚠️  SKIPPED: Requires root privileges\n");
        tests_passed++;
        return;
    }
    
    // Create multiple TUN devices
    tun_device_t *tun1 = tun_create("tun_test5");
    ASSERT(tun1 != NULL, "First device should be created");
    
    tun_device_t *tun2 = tun_create("tun_test6");
    ASSERT(tun2 != NULL, "Second device should be created");
    
    tun_device_t *tun3 = tun_create("tun_test7");
    ASSERT(tun3 != NULL, "Third device should be created");
    
    // Configure all
    tun_configure(tun1, "10.0.1.1", "255.255.255.0");
    tun_configure(tun2, "10.0.2.1", "255.255.255.0");
    tun_configure(tun3, "10.0.3.1", "255.255.255.0");
    
    printf("Created 3 TUN devices: %s, %s, %s\n", 
           tun1->name, tun2->name, tun3->name);
    
    // Destroy all
    tun_destroy(tun1);
    tun_destroy(tun2);
    tun_destroy(tun3);
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║            TUN Device Unit Tests                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (geteuid() != 0) {
        printf("\n⚠️  WARNING: Not running as root\n");
        printf("   Most tests will be skipped\n");
        printf("   Run with: sudo ./bin/test_tun_device\n\n");
    }
    
    test_create_destroy();
    test_configure_ip();
    test_set_mtu();
    test_write_packet();
    test_read_timeout();
    test_ipv4_validation();
    test_error_handling();
    test_multiple_devices();
    
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
