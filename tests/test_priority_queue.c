/**
 * test_priority_queue.c
 * 
 * Unit tests for Priority Queue
 */

#include "../src/common/priority_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
    
    priority_queue_t *pq = pq_create(100);
    ASSERT(pq != NULL, "Creation should succeed");
    ASSERT(pq_size(pq) == 0, "Initial size should be 0");
    ASSERT(pq_is_empty(pq), "Should be empty");
    ASSERT(!pq_is_full(pq), "Should not be full");
    
    printf("Created queue with capacity 100\n");
    
    pq_destroy(pq);
    
    TEST_PASS();
}

/**
 * Test 2: Enqueue and Dequeue
 */
void test_enqueue_dequeue(void) {
    TEST_START("Enqueue and Dequeue");
    
    priority_queue_t *pq = pq_create(10);
    
    // Enqueue packets
    uint8_t packet1[10] = {1,2,3,4,5,6,7,8,9,10};
    uint8_t packet2[20] = {11,12,13,14,15,16,17,18,19,20};
    
    int ret = pq_enqueue(pq, packet1, 10, PRIORITY_NORMAL);
    ASSERT(ret == PQ_SUCCESS, "Enqueue should succeed");
    ASSERT(pq_size(pq) == 1, "Size should be 1");
    
    ret = pq_enqueue(pq, packet2, 20, PRIORITY_NORMAL);
    ASSERT(ret == PQ_SUCCESS, "Second enqueue should succeed");
    ASSERT(pq_size(pq) == 2, "Size should be 2");
    
    // Dequeue packets
    uint8_t *dequeued;
    size_t size;
    priority_level_t priority;
    
    ret = pq_dequeue(pq, &dequeued, &size, &priority);
    ASSERT(ret == PQ_SUCCESS, "Dequeue should succeed");
    ASSERT(size == 10, "Size should be 10");
    ASSERT(priority == PRIORITY_NORMAL, "Priority should be NORMAL");
    ASSERT(memcmp(dequeued, packet1, 10) == 0, "Data should match");
    free(dequeued);
    
    ASSERT(pq_size(pq) == 1, "Size should be 1 after dequeue");
    
    ret = pq_dequeue(pq, &dequeued, &size, &priority);
    ASSERT(ret == PQ_SUCCESS, "Second dequeue should succeed");
    ASSERT(size == 20, "Size should be 20");
    ASSERT(memcmp(dequeued, packet2, 20) == 0, "Data should match");
    free(dequeued);
    
    ASSERT(pq_is_empty(pq), "Queue should be empty");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 3: Priority Ordering
 */
void test_priority_ordering(void) {
    TEST_START("Priority Ordering");
    
    priority_queue_t *pq = pq_create(100);
    
    // Enqueue packets with different priorities
    uint8_t low[10] = "LOW";
    uint8_t normal[10] = "NORMAL";
    uint8_t high[10] = "HIGH";
    
    pq_enqueue(pq, low, 10, PRIORITY_LOW);
    pq_enqueue(pq, high, 10, PRIORITY_HIGH);
    pq_enqueue(pq, normal, 10, PRIORITY_NORMAL);
    
    ASSERT(pq_size(pq) == 3, "Should have 3 packets");
    
    // Dequeue should get HIGH first
    uint8_t *packet;
    size_t size;
    priority_level_t priority;
    
    pq_dequeue(pq, &packet, &size, &priority);
    ASSERT(priority == PRIORITY_HIGH, "First should be HIGH");
    ASSERT(memcmp(packet, high, 10) == 0, "Data should match HIGH");
    free(packet);
    
    // Then NORMAL
    pq_dequeue(pq, &packet, &size, &priority);
    ASSERT(priority == PRIORITY_NORMAL, "Second should be NORMAL");
    ASSERT(memcmp(packet, normal, 10) == 0, "Data should match NORMAL");
    free(packet);
    
    // Then LOW
    pq_dequeue(pq, &packet, &size, &priority);
    ASSERT(priority == PRIORITY_LOW, "Third should be LOW");
    ASSERT(memcmp(packet, low, 10) == 0, "Data should match LOW");
    free(packet);
    
    printf("Priority ordering: HIGH → NORMAL → LOW ✓\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 4: FIFO within Same Priority
 */
void test_fifo_same_priority(void) {
    TEST_START("FIFO within Same Priority");
    
    priority_queue_t *pq = pq_create(100);
    
    // Enqueue multiple packets with same priority
    uint8_t packet1[10] = "FIRST";
    uint8_t packet2[10] = "SECOND";
    uint8_t packet3[10] = "THIRD";
    
    pq_enqueue(pq, packet1, 10, PRIORITY_NORMAL);
    pq_enqueue(pq, packet2, 10, PRIORITY_NORMAL);
    pq_enqueue(pq, packet3, 10, PRIORITY_NORMAL);
    
    // Dequeue should follow FIFO order
    uint8_t *packet;
    size_t size;
    
    pq_dequeue(pq, &packet, &size, NULL);
    ASSERT(memcmp(packet, packet1, 10) == 0, "First should be FIRST");
    free(packet);
    
    pq_dequeue(pq, &packet, &size, NULL);
    ASSERT(memcmp(packet, packet2, 10) == 0, "Second should be SECOND");
    free(packet);
    
    pq_dequeue(pq, &packet, &size, NULL);
    ASSERT(memcmp(packet, packet3, 10) == 0, "Third should be THIRD");
    free(packet);
    
    printf("FIFO order maintained within same priority ✓\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 5: Full Queue
 */
void test_full_queue(void) {
    TEST_START("Full Queue");
    
    priority_queue_t *pq = pq_create(5);  // Small capacity
    
    uint8_t packet[10] = "TEST";
    
    // Fill queue
    for (int i = 0; i < 5; i++) {
        int ret = pq_enqueue(pq, packet, 10, PRIORITY_NORMAL);
        ASSERT(ret == PQ_SUCCESS, "Enqueue should succeed");
    }
    
    ASSERT(pq_is_full(pq), "Queue should be full");
    
    // Try to enqueue when full
    int ret = pq_enqueue(pq, packet, 10, PRIORITY_NORMAL);
    ASSERT(ret == PQ_ERROR_FULL, "Enqueue should fail when full");
    
    // Check dropped count
    uint64_t dropped;
    pq_get_stats(pq, NULL, NULL, &dropped);
    ASSERT(dropped == 1, "Should have 1 dropped packet");
    
    printf("Full queue correctly rejects new packets\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 6: Peek Operation
 */
void test_peek(void) {
    TEST_START("Peek Operation");
    
    priority_queue_t *pq = pq_create(100);
    
    uint8_t packet1[10] = "LOW";
    uint8_t packet2[10] = "HIGH";
    
    pq_enqueue(pq, packet1, 10, PRIORITY_LOW);
    pq_enqueue(pq, packet2, 10, PRIORITY_HIGH);
    
    // Peek at highest priority
    size_t size;
    priority_level_t priority;
    const uint8_t *peeked = pq_peek(pq, &size, &priority);
    
    ASSERT(peeked != NULL, "Peek should succeed");
    ASSERT(size == 10, "Size should be 10");
    ASSERT(priority == PRIORITY_HIGH, "Should be HIGH priority");
    ASSERT(memcmp(peeked, packet2, 10) == 0, "Data should match HIGH");
    
    // Queue should not change
    ASSERT(pq_size(pq) == 2, "Size should still be 2");
    
    printf("Peek operation does not modify queue ✓\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 7: Clear Queue
 */
void test_clear(void) {
    TEST_START("Clear Queue");
    
    priority_queue_t *pq = pq_create(100);
    
    uint8_t packet[10] = "TEST";
    
    // Add some packets
    for (int i = 0; i < 10; i++) {
        pq_enqueue(pq, packet, 10, PRIORITY_NORMAL);
    }
    
    ASSERT(pq_size(pq) == 10, "Should have 10 packets");
    
    // Clear queue
    size_t cleared = pq_clear(pq);
    ASSERT(cleared == 10, "Should clear 10 packets");
    ASSERT(pq_size(pq) == 0, "Queue should be empty");
    ASSERT(pq_is_empty(pq), "Should be empty");
    
    printf("Cleared 10 packets from queue\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 8: Statistics
 */
void test_statistics(void) {
    TEST_START("Statistics");
    
    priority_queue_t *pq = pq_create(100);
    
    uint8_t packet[10] = "TEST";
    
    // Enqueue packets with different priorities
    pq_enqueue(pq, packet, 10, PRIORITY_HIGH);
    pq_enqueue(pq, packet, 10, PRIORITY_HIGH);
    pq_enqueue(pq, packet, 10, PRIORITY_NORMAL);
    pq_enqueue(pq, packet, 10, PRIORITY_LOW);
    
    // Check global stats
    uint64_t enqueued, dequeued, dropped;
    pq_get_stats(pq, &enqueued, &dequeued, &dropped);
    
    ASSERT(enqueued == 4, "Should have enqueued 4");
    ASSERT(dequeued == 0, "Should have dequeued 0");
    ASSERT(dropped == 0, "Should have dropped 0");
    
    // Check priority stats
    uint64_t high_enq, high_deq;
    pq_get_priority_stats(pq, PRIORITY_HIGH, &high_enq, &high_deq);
    ASSERT(high_enq == 2, "Should have 2 HIGH enqueued");
    ASSERT(high_deq == 0, "Should have 0 HIGH dequeued");
    
    // Dequeue one
    uint8_t *deq;
    size_t size;
    pq_dequeue(pq, &deq, &size, NULL);
    free(deq);
    
    // Check updated stats
    pq_get_stats(pq, &enqueued, &dequeued, &dropped);
    ASSERT(dequeued == 1, "Should have dequeued 1");
    
    pq_get_priority_stats(pq, PRIORITY_HIGH, &high_enq, &high_deq);
    ASSERT(high_deq == 1, "Should have 1 HIGH dequeued");
    
    printf("Statistics: %llu enqueued, %llu dequeued\n", 
           (unsigned long long)enqueued, (unsigned long long)dequeued);
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Test 9: Error Handling
 */
void test_error_handling(void) {
    TEST_START("Error Handling");
    
    // NULL pointer tests
    priority_queue_t *pq = pq_create(10);
    
    uint8_t packet[10] = "TEST";
    int ret = pq_enqueue(NULL, packet, 10, PRIORITY_NORMAL);
    ASSERT(ret == PQ_ERROR_INVALID_PARAM, "NULL queue should fail");
    
    ret = pq_enqueue(pq, NULL, 10, PRIORITY_NORMAL);
    ASSERT(ret == PQ_ERROR_INVALID_PARAM, "NULL packet should fail");
    
    // Empty queue dequeue
    uint8_t *deq;
    size_t size;
    ret = pq_dequeue(pq, &deq, &size, NULL);
    ASSERT(ret == PQ_ERROR_EMPTY, "Dequeue empty should fail");
    
    // Test error strings
    const char *err = pq_error_string(PQ_SUCCESS);
    ASSERT(strcmp(err, "Success") == 0, "Error string should match");
    
    err = pq_error_string(PQ_ERROR_FULL);
    ASSERT(strcmp(err, "Queue full") == 0, "Error string should match");
    
    // Test priority strings
    const char *pri = pq_priority_string(PRIORITY_HIGH);
    ASSERT(strcmp(pri, "HIGH") == 0, "Priority string should match");
    
    printf("Error handling validated\n");
    
    pq_destroy(pq);
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         Priority Queue Unit Tests                         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_create_destroy();
    test_enqueue_dequeue();
    test_priority_ordering();
    test_fifo_same_priority();
    test_full_queue();
    test_peek();
    test_clear();
    test_statistics();
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
