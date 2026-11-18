/**
 * priority_queue.h
 * 
 * Priority Queue for ICMP Packet Scheduling
 * 
 * Features:
 * - 3 priority levels (HIGH, NORMAL, LOW)
 * - Thread-safe operations
 * - Fixed capacity with overflow handling
 * - FIFO within same priority
 */

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

// Constants
#define PQ_MAX_CAPACITY 10000            // Maximum queue capacity
#define PQ_DEFAULT_CAPACITY 1000         // Default capacity

// Priority levels
typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH = 2,
    PRIORITY_COUNT = 3                   // Number of priority levels
} priority_level_t;

// Error codes
#define PQ_SUCCESS 0
#define PQ_ERROR_INVALID_PARAM -1
#define PQ_ERROR_FULL -2
#define PQ_ERROR_EMPTY -3
#define PQ_ERROR_MEMORY -4

/**
 * Packet entry in queue
 */
typedef struct {
    uint8_t *data;                       // Packet data (malloc'd)
    size_t size;                         // Packet size
    priority_level_t priority;           // Priority level
    uint64_t sequence;                   // Sequence number (for FIFO)
} pq_entry_t;

/**
 * Priority queue context
 */
typedef struct {
    pq_entry_t *entries;                 // Array of entries
    size_t capacity;                     // Maximum capacity
    size_t count;                        // Current count
    uint64_t sequence_counter;           // Global sequence counter
    
    // Statistics
    uint64_t total_enqueued;             // Total packets enqueued
    uint64_t total_dequeued;             // Total packets dequeued
    uint64_t enqueued_by_priority[PRIORITY_COUNT];
    uint64_t dequeued_by_priority[PRIORITY_COUNT];
    uint64_t dropped;                    // Dropped due to full queue
    
    pthread_mutex_t lock;                // Thread-safe lock
} priority_queue_t;

/**
 * Create priority queue
 * 
 * @param capacity Maximum queue capacity (0 for default)
 * 
 * @return Pointer to priority_queue_t on success, NULL on error
 * 
 * Example:
 *   priority_queue_t *pq = pq_create(1000);
 *   if (pq) {
 *       // Use queue
 *       pq_destroy(pq);
 *   }
 */
priority_queue_t* pq_create(size_t capacity);

/**
 * Enqueue packet with priority
 * 
 * @param pq Priority queue
 * @param packet Packet data (will be copied)
 * @param packet_size Size of packet
 * @param priority Priority level
 * 
 * @return PQ_SUCCESS on success, error code otherwise
 * 
 * Note: Packet data is copied internally
 * 
 * Example:
 *   uint8_t packet[100];
 *   pq_enqueue(pq, packet, 100, PRIORITY_HIGH);
 */
int pq_enqueue(priority_queue_t *pq, const uint8_t *packet, size_t packet_size,
                priority_level_t priority);

/**
 * Dequeue highest priority packet
 * 
 * @param pq Priority queue
 * @param packet Output: packet data (caller must free)
 * @param packet_size Output: size of packet
 * @param priority Output: priority of packet (optional, can be NULL)
 * 
 * @return PQ_SUCCESS on success, PQ_ERROR_EMPTY if empty
 * 
 * Note: Caller must free packet data with free()
 * 
 * Example:
 *   uint8_t *packet;
 *   size_t size;
 *   priority_level_t priority;
 *   if (pq_dequeue(pq, &packet, &size, &priority) == PQ_SUCCESS) {
 *       // Use packet
 *       free(packet);
 *   }
 */
int pq_dequeue(priority_queue_t *pq, uint8_t **packet, size_t *packet_size,
                priority_level_t *priority);

/**
 * Peek at highest priority packet (without removing)
 * 
 * @param pq Priority queue
 * @param packet_size Output: size of packet (optional, can be NULL)
 * @param priority Output: priority of packet (optional, can be NULL)
 * 
 * @return Pointer to packet data (internal, do not free), NULL if empty
 * 
 * Note: Do not modify or free returned pointer
 * 
 * Example:
 *   size_t size;
 *   priority_level_t priority;
 *   const uint8_t *packet = pq_peek(pq, &size, &priority);
 *   if (packet) {
 *       printf("Next packet: %zu bytes, priority %d\n", size, priority);
 *   }
 */
const uint8_t* pq_peek(priority_queue_t *pq, size_t *packet_size,
                        priority_level_t *priority);

/**
 * Get queue size
 * 
 * @param pq Priority queue
 * 
 * @return Number of packets in queue
 */
size_t pq_size(priority_queue_t *pq);

/**
 * Check if queue is empty
 * 
 * @param pq Priority queue
 * 
 * @return 1 if empty, 0 otherwise
 */
int pq_is_empty(priority_queue_t *pq);

/**
 * Check if queue is full
 * 
 * @param pq Priority queue
 * 
 * @return 1 if full, 0 otherwise
 */
int pq_is_full(priority_queue_t *pq);

/**
 * Clear all packets from queue
 * 
 * @param pq Priority queue
 * 
 * @return Number of packets cleared
 */
size_t pq_clear(priority_queue_t *pq);

/**
 * Get queue statistics
 * 
 * @param pq Priority queue
 * @param total_enqueued Output: total packets enqueued
 * @param total_dequeued Output: total packets dequeued
 * @param dropped Output: packets dropped due to full queue
 * 
 * @return PQ_SUCCESS on success, error code otherwise
 */
int pq_get_stats(priority_queue_t *pq, uint64_t *total_enqueued,
                  uint64_t *total_dequeued, uint64_t *dropped);

/**
 * Get statistics by priority
 * 
 * @param pq Priority queue
 * @param priority Priority level
 * @param enqueued Output: packets enqueued at this priority
 * @param dequeued Output: packets dequeued at this priority
 * 
 * @return PQ_SUCCESS on success, error code otherwise
 */
int pq_get_priority_stats(priority_queue_t *pq, priority_level_t priority,
                           uint64_t *enqueued, uint64_t *dequeued);

/**
 * Destroy priority queue
 * 
 * @param pq Priority queue
 * 
 * Note: Frees all remaining packets and queue memory
 */
void pq_destroy(priority_queue_t *pq);

/**
 * Get priority name string
 * 
 * @param priority Priority level
 * @return Human-readable priority name
 */
const char* pq_priority_string(priority_level_t priority);

/**
 * Get error message string
 * 
 * @param error_code Error code
 * @return Human-readable error message
 */
const char* pq_error_string(int error_code);

#endif // PRIORITY_QUEUE_H
