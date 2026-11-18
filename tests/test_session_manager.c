/**
 * test_session_manager.c
 * 
 * Unit tests for Session Manager
 */

#include "../src/server/session_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <arpa/inet.h>

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
 * Helper: Create test client address
 */
void create_test_addr(struct sockaddr_in *addr, const char *ip, uint16_t port) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr->sin_addr);
}

/**
 * Test 1: Initialization and Destruction
 */
void test_init_destroy(void) {
    TEST_START("Initialization and Destruction");
    
    session_manager_t manager;
    
    // Initialize
    int ret = session_manager_init(&manager);
    ASSERT(ret == SESSION_SUCCESS, "Init should succeed");
    ASSERT(manager.active_count == 0, "Initial count should be 0");
    
    // Check all sessions inactive
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        ASSERT(manager.sessions[i].active == 0, "Session should be inactive");
    }
    
    // Destroy
    session_manager_destroy(&manager);
    ASSERT(manager.active_count == 0, "Count should be 0 after destroy");
    
    TEST_PASS();
}

/**
 * Test 2: Create and Remove Session
 */
void test_create_remove(void) {
    TEST_START("Create and Remove Session");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    struct sockaddr_in client_addr;
    create_test_addr(&client_addr, "192.168.1.100", 12345);
    
    // Create session
    int ret = session_manager_create(&manager, 0x1234, &client_addr);
    ASSERT(ret == SESSION_SUCCESS, "Create should succeed");
    ASSERT(manager.active_count == 1, "Active count should be 1");
    
    // Try to create duplicate
    ret = session_manager_create(&manager, 0x1234, &client_addr);
    ASSERT(ret == SESSION_ERROR_ALREADY_EXISTS, "Duplicate should fail");
    
    // Get session info
    session_entry_t info;
    ret = session_manager_get_info(&manager, 0x1234, &info);
    ASSERT(ret == SESSION_SUCCESS, "Get info should succeed");
    ASSERT(info.session_id == 0x1234, "Session ID should match");
    ASSERT(info.state == SESSION_STATE_INIT, "State should be INIT");
    ASSERT(info.key_established == 0, "Key should not be established");
    ASSERT(info.active == 1, "Session should be active");
    
    // Remove session
    ret = session_manager_remove(&manager, 0x1234);
    ASSERT(ret == SESSION_SUCCESS, "Remove should succeed");
    ASSERT(manager.active_count == 0, "Active count should be 0");
    
    // Try to get removed session
    ret = session_manager_get_info(&manager, 0x1234, &info);
    ASSERT(ret == SESSION_ERROR_NOT_FOUND, "Removed session should not be found");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 3: Set and Get Key
 */
void test_set_get_key(void) {
    TEST_START("Set and Get Key");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    struct sockaddr_in client_addr;
    create_test_addr(&client_addr, "192.168.1.100", 12345);
    
    // Create session
    session_manager_create(&manager, 0x1234, &client_addr);
    
    // Set key
    uint8_t key[32];
    for (int i = 0; i < 32; i++) {
        key[i] = i;
    }
    
    int ret = session_manager_set_key(&manager, 0x1234, key);
    ASSERT(ret == SESSION_SUCCESS, "Set key should succeed");
    
    // Get key
    uint8_t retrieved_key[32];
    ret = session_manager_get_key(&manager, 0x1234, retrieved_key);
    ASSERT(ret == SESSION_SUCCESS, "Get key should succeed");
    
    // Verify key matches
    ASSERT(memcmp(key, retrieved_key, 32) == 0, "Keys should match");
    
    // Check state changed to ACTIVE
    session_entry_t info;
    session_manager_get_info(&manager, 0x1234, &info);
    ASSERT(info.state == SESSION_STATE_ACTIVE, "State should be ACTIVE");
    ASSERT(info.key_established == 1, "Key should be established");
    
    // Try to get key for non-existent session
    ret = session_manager_get_key(&manager, 0x9999, retrieved_key);
    ASSERT(ret == SESSION_ERROR_NOT_FOUND, "Non-existent session should fail");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 4: Find by Address
 */
void test_find_by_addr(void) {
    TEST_START("Find by Address");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    // Create multiple sessions
    struct sockaddr_in addr1, addr2, addr3;
    create_test_addr(&addr1, "192.168.1.100", 12345);
    create_test_addr(&addr2, "192.168.1.101", 12346);
    create_test_addr(&addr3, "192.168.1.102", 12347);
    
    session_manager_create(&manager, 0x1111, &addr1);
    session_manager_create(&manager, 0x2222, &addr2);
    session_manager_create(&manager, 0x3333, &addr3);
    
    ASSERT(manager.active_count == 3, "Should have 3 active sessions");
    
    // Find by address
    uint16_t session_id;
    int ret = session_manager_find_by_addr(&manager, &addr1, &session_id);
    ASSERT(ret == SESSION_SUCCESS, "Find should succeed");
    ASSERT(session_id == 0x1111, "Session ID should be 0x1111");
    
    ret = session_manager_find_by_addr(&manager, &addr2, &session_id);
    ASSERT(ret == SESSION_SUCCESS, "Find should succeed");
    ASSERT(session_id == 0x2222, "Session ID should be 0x2222");
    
    ret = session_manager_find_by_addr(&manager, &addr3, &session_id);
    ASSERT(ret == SESSION_SUCCESS, "Find should succeed");
    ASSERT(session_id == 0x3333, "Session ID should be 0x3333");
    
    // Try non-existent address
    struct sockaddr_in addr_unknown;
    create_test_addr(&addr_unknown, "10.0.0.1", 9999);
    ret = session_manager_find_by_addr(&manager, &addr_unknown, &session_id);
    ASSERT(ret == SESSION_ERROR_NOT_FOUND, "Unknown address should not be found");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 5: Update Activity
 */
void test_update_activity(void) {
    TEST_START("Update Activity");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    struct sockaddr_in client_addr;
    create_test_addr(&client_addr, "192.168.1.100", 12345);
    
    // Create session
    session_manager_create(&manager, 0x1234, &client_addr);
    
    // Get initial time
    session_entry_t info1;
    session_manager_get_info(&manager, 0x1234, &info1);
    time_t time1 = info1.last_activity;
    
    // Wait 2 seconds
    sleep(2);
    
    // Update activity
    int ret = session_manager_update_activity(&manager, 0x1234);
    ASSERT(ret == SESSION_SUCCESS, "Update activity should succeed");
    
    // Get updated time
    session_entry_t info2;
    session_manager_get_info(&manager, 0x1234, &info2);
    time_t time2 = info2.last_activity;
    
    // Verify time updated
    ASSERT(time2 > time1, "Last activity should be updated");
    ASSERT((time2 - time1) >= 2, "Time difference should be at least 2 seconds");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 6: Update Statistics
 */
void test_update_stats(void) {
    TEST_START("Update Statistics");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    struct sockaddr_in client_addr;
    create_test_addr(&client_addr, "192.168.1.100", 12345);
    
    // Create session
    session_manager_create(&manager, 0x1234, &client_addr);
    
    // Update stats multiple times
    int ret = session_manager_update_stats(&manager, 0x1234, 10, 5, 1000, 500);
    ASSERT(ret == SESSION_SUCCESS, "Update stats should succeed");
    
    ret = session_manager_update_stats(&manager, 0x1234, 15, 8, 1500, 800);
    ASSERT(ret == SESSION_SUCCESS, "Second update should succeed");
    
    // Get stats
    session_entry_t info;
    session_manager_get_info(&manager, 0x1234, &info);
    
    ASSERT(info.packets_received == 25, "Packets received should be 25");
    ASSERT(info.packets_sent == 13, "Packets sent should be 13");
    ASSERT(info.bytes_received == 2500, "Bytes received should be 2500");
    ASSERT(info.bytes_sent == 1300, "Bytes sent should be 1300");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 7: Cleanup Expired Sessions
 */
void test_cleanup_timeout(void) {
    TEST_START("Cleanup Expired Sessions");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    // Create sessions
    struct sockaddr_in addr1, addr2;
    create_test_addr(&addr1, "192.168.1.100", 12345);
    create_test_addr(&addr2, "192.168.1.101", 12346);
    
    session_manager_create(&manager, 0x1111, &addr1);
    session_manager_create(&manager, 0x2222, &addr2);
    
    ASSERT(manager.active_count == 2, "Should have 2 active sessions");
    
    // Simulate timeout by manually setting last_activity in the past
    // (In real scenario, we'd wait SESSION_TIMEOUT_SEC seconds)
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager.sessions[i].active && manager.sessions[i].session_id == 0x1111) {
            manager.sessions[i].last_activity = time(NULL) - SESSION_TIMEOUT_SEC - 10;
            break;
        }
    }
    
    // Run cleanup
    int removed = session_manager_cleanup(&manager);
    ASSERT(removed == 1, "Should remove 1 session");
    ASSERT(manager.active_count == 1, "Should have 1 active session");
    
    // Verify only session 0x2222 remains
    session_entry_t info;
    int ret = session_manager_get_info(&manager, 0x1111, &info);
    ASSERT(ret == SESSION_ERROR_NOT_FOUND, "Expired session should be removed");
    
    ret = session_manager_get_info(&manager, 0x2222, &info);
    ASSERT(ret == SESSION_SUCCESS, "Active session should remain");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 8: Get All Sessions
 */
void test_get_all_sessions(void) {
    TEST_START("Get All Sessions");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    // Create multiple sessions
    struct sockaddr_in addr;
    for (int i = 0; i < 5; i++) {
        char ip[32];
        snprintf(ip, sizeof(ip), "192.168.1.%d", 100 + i);
        create_test_addr(&addr, ip, 12345 + i);
        session_manager_create(&manager, 0x1000 + i, &addr);
    }
    
    ASSERT(manager.active_count == 5, "Should have 5 active sessions");
    
    // Get all sessions
    session_entry_t sessions[10];
    size_t count;
    int ret = session_manager_get_all(&manager, sessions, &count, 10);
    ASSERT(ret == SESSION_SUCCESS, "Get all should succeed");
    ASSERT(count == 5, "Should return 5 sessions");
    
    // Verify session IDs
    int found_sessions = 0;
    for (size_t i = 0; i < count; i++) {
        if (sessions[i].session_id >= 0x1000 && sessions[i].session_id <= 0x1004) {
            found_sessions++;
        }
    }
    ASSERT(found_sessions == 5, "Should find all 5 sessions");
    
    // Test with limited buffer
    session_entry_t limited_sessions[3];
    size_t limited_count;
    ret = session_manager_get_all(&manager, limited_sessions, &limited_count, 3);
    ASSERT(ret == SESSION_SUCCESS, "Get all with limit should succeed");
    ASSERT(limited_count == 3, "Should return only 3 sessions");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 9: Session Count
 */
void test_session_count(void) {
    TEST_START("Session Count");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    ASSERT(session_manager_count(&manager) == 0, "Initial count should be 0");
    
    // Create sessions
    struct sockaddr_in addr;
    for (int i = 0; i < 10; i++) {
        char ip[32];
        snprintf(ip, sizeof(ip), "192.168.1.%d", 100 + i);
        create_test_addr(&addr, ip, 12345 + i);
        session_manager_create(&manager, 0x1000 + i, &addr);
    }
    
    ASSERT(session_manager_count(&manager) == 10, "Count should be 10");
    
    // Remove some sessions
    session_manager_remove(&manager, 0x1000);
    session_manager_remove(&manager, 0x1001);
    
    ASSERT(session_manager_count(&manager) == 8, "Count should be 8");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Test 10: Error Handling
 */
void test_error_handling(void) {
    TEST_START("Error Handling");
    
    session_manager_t manager;
    session_manager_init(&manager);
    
    // Test invalid parameters
    int ret = session_manager_init(NULL);
    ASSERT(ret == SESSION_ERROR_INVALID_PARAM, "NULL init should fail");
    
    ret = session_manager_create(NULL, 0x1234, NULL);
    ASSERT(ret == SESSION_ERROR_INVALID_PARAM, "NULL create should fail");
    
    uint8_t key[32];
    ret = session_manager_set_key(NULL, 0x1234, key);
    ASSERT(ret == SESSION_ERROR_INVALID_PARAM, "NULL set_key should fail");
    
    ret = session_manager_get_key(&manager, 0x9999, key);
    ASSERT(ret == SESSION_ERROR_NOT_FOUND, "Non-existent get_key should fail");
    
    // Test error strings
    const char *err_str = session_manager_error_string(SESSION_SUCCESS);
    ASSERT(strcmp(err_str, "Success") == 0, "Error string should be 'Success'");
    
    err_str = session_manager_error_string(SESSION_ERROR_NOT_FOUND);
    ASSERT(strcmp(err_str, "Session not found") == 0, "Error string should match");
    
    // Test state strings
    const char *state_str = session_state_string(SESSION_STATE_ACTIVE);
    ASSERT(strcmp(state_str, "ACTIVE") == 0, "State string should be 'ACTIVE'");
    
    session_manager_destroy(&manager);
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         Session Manager Unit Tests                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_init_destroy();
    test_create_remove();
    test_set_get_key();
    test_find_by_addr();
    test_update_activity();
    test_update_stats();
    test_cleanup_timeout();
    test_get_all_sessions();
    test_session_count();
    test_error_handling();
    
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
