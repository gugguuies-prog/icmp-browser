/**
 * priority_queue.c
 * 
 * Priority Queue Implementation
 */

#include "priority_queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Create priority queue
 */
priority_queue_t* pq_create(size_t capacity) {
    if (capacity == 0) {
        capacity = PQ_DEFAULT_CAPACITY;
    }
    if (capacity > PQ_MAX_CAPACITY) {
        capacity = PQ_MAX_CAPACITY;
    }
    
    priority_queue_t *pq = (priority_queue_t*)malloc(sizeof(priority_queue_t));
    if (!pq) {
        return NULL;
    }
    
    memset(pq, 0, sizeof(priority_queue_t));
    
    pq->entries = (pq_entry_t*)calloc(capacity, sizeof(pq_entry_t));
    if (!pq->entries) {
        free(pq);
        return NULL;
    }
    
    pq->capacity = capacity;
    pq->count = 0;
    pq->sequence_counter = 0;
    
    if (pthread_mutex_init(&pq->lock, NULL) != 0) {
        free(pq->entries);
        free(pq);
        return NULL;
    }
    
    return pq;
}

/**
 * Enqueue packet with priority
 */
int pq_enqueue(priority_queue_t *pq, const uint8_t *packet, size_t packet_size,
                priority_level_t priority) {
    if (!pq || !packet || packet_size == 0) {
        return PQ_ERROR_INVALID_PARAM;
    }
    
    if (priority < 0 || priority >= PRIORITY_COUNT) {
        return PQ_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    // Check if queue is full
    if (pq->count >= pq->capacity) {
        pq->dropped++;
        pthread_mutex_unlock(&pq->lock);
        return PQ_ERROR_FULL;
    }
    
    // Allocate and copy packet data
    uint8_t *data = (uint8_t*)malloc(packet_size);
    if (!data) {
        pthread_mutex_unlock(&pq->lock);
        return PQ_ERROR_MEMORY;
    }
    memcpy(data, packet, packet_size);
    
    // Add to queue
    pq_entry_t *entry = &pq->entries[pq->count];
    entry->data = data;
    entry->size = packet_size;
    entry->priority = priority;
    entry->sequence = pq->sequence_counter++;
    
    pq->count++;
    pq->total_enqueued++;
    pq->enqueued_by_priority[priority]++;
    
    pthread_mutex_unlock(&pq->lock);
    
    return PQ_SUCCESS;
}

/**
 * Find highest priority packet index
 */
static size_t pq_find_highest_priority(priority_queue_t *pq) {
    if (pq->count == 0) {
        return (size_t)-1;
    }
    
    size_t best_idx = 0;
    pq_entry_t *best = &pq->entries[0];
    
    for (size_t i = 1; i < pq->count; i++) {
        pq_entry_t *current = &pq->entries[i];
        
        // Higher priority wins
        if (current->priority > best->priority) {
            best_idx = i;
            best = current;
        }
        // Same priority: FIFO (lower sequence wins)
        else if (current->priority == best->priority && 
                 current->sequence < best->sequence) {
            best_idx = i;
            best = current;
        }
    }
    
    return best_idx;
}

/**
 * Dequeue highest priority packet
 */
int pq_dequeue(priority_queue_t *pq, uint8_t **packet, size_t *packet_size,
                priority_level_t *priority) {
    if (!pq || !packet || !packet_size) {
        return PQ_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    if (pq->count == 0) {
        pthread_mutex_unlock(&pq->lock);
        return PQ_ERROR_EMPTY;
    }
    
    // Find highest priority packet
    size_t idx = pq_find_highest_priority(pq);
    pq_entry_t *entry = &pq->entries[idx];
    
    // Return packet data
    *packet = entry->data;
    *packet_size = entry->size;
    if (priority) {
        *priority = entry->priority;
    }
    
    // Update statistics
    pq->total_dequeued++;
    pq->dequeued_by_priority[entry->priority]++;
    
    // Remove from queue (shift remaining entries)
    if (idx < pq->count - 1) {
        memmove(&pq->entries[idx], &pq->entries[idx + 1],
                (pq->count - idx - 1) * sizeof(pq_entry_t));
    }
    
    pq->count--;
    
    pthread_mutex_unlock(&pq->lock);
    
    return PQ_SUCCESS;
}

/**
 * Peek at highest priority packet
 */
const uint8_t* pq_peek(priority_queue_t *pq, size_t *packet_size,
                        priority_level_t *priority) {
    if (!pq) {
        return NULL;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    if (pq->count == 0) {
        pthread_mutex_unlock(&pq->lock);
        return NULL;
    }
    
    size_t idx = pq_find_highest_priority(pq);
    pq_entry_t *entry = &pq->entries[idx];
    
    if (packet_size) {
        *packet_size = entry->size;
    }
    if (priority) {
        *priority = entry->priority;
    }
    
    const uint8_t *data = entry->data;
    
    pthread_mutex_unlock(&pq->lock);
    
    return data;
}

/**
 * Get queue size
 */
size_t pq_size(priority_queue_t *pq) {
    if (!pq) {
        return 0;
    }
    
    pthread_mutex_lock(&pq->lock);
    size_t count = pq->count;
    pthread_mutex_unlock(&pq->lock);
    
    return count;
}

/**
 * Check if queue is empty
 */
int pq_is_empty(priority_queue_t *pq) {
    return pq_size(pq) == 0;
}

/**
 * Check if queue is full
 */
int pq_is_full(priority_queue_t *pq) {
    if (!pq) {
        return 1;
    }
    
    pthread_mutex_lock(&pq->lock);
    int full = (pq->count >= pq->capacity);
    pthread_mutex_unlock(&pq->lock);
    
    return full;
}

/**
 * Clear all packets from queue
 */
size_t pq_clear(priority_queue_t *pq) {
    if (!pq) {
        return 0;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    // Free all packet data
    for (size_t i = 0; i < pq->count; i++) {
        if (pq->entries[i].data) {
            free(pq->entries[i].data);
            pq->entries[i].data = NULL;
        }
    }
    
    size_t cleared = pq->count;
    pq->count = 0;
    
    pthread_mutex_unlock(&pq->lock);
    
    return cleared;
}

/**
 * Get queue statistics
 */
int pq_get_stats(priority_queue_t *pq, uint64_t *total_enqueued,
                  uint64_t *total_dequeued, uint64_t *dropped) {
    if (!pq) {
        return PQ_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    if (total_enqueued) {
        *total_enqueued = pq->total_enqueued;
    }
    if (total_dequeued) {
        *total_dequeued = pq->total_dequeued;
    }
    if (dropped) {
        *dropped = pq->dropped;
    }
    
    pthread_mutex_unlock(&pq->lock);
    
    return PQ_SUCCESS;
}

/**
 * Get statistics by priority
 */
int pq_get_priority_stats(priority_queue_t *pq, priority_level_t priority,
                           uint64_t *enqueued, uint64_t *dequeued) {
    if (!pq || priority < 0 || priority >= PRIORITY_COUNT) {
        return PQ_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pq->lock);
    
    if (enqueued) {
        *enqueued = pq->enqueued_by_priority[priority];
    }
    if (dequeued) {
        *dequeued = pq->dequeued_by_priority[priority];
    }
    
    pthread_mutex_unlock(&pq->lock);
    
    return PQ_SUCCESS;
}

/**
 * Destroy priority queue
 */
void pq_destroy(priority_queue_t *pq) {
    if (!pq) {
        return;
    }
    
    // Clear all packets
    pq_clear(pq);
    
    // Free entries array
    if (pq->entries) {
        free(pq->entries);
    }
    
    pthread_mutex_destroy(&pq->lock);
    
    free(pq);
}

/**
 * Get priority name string
 */
const char* pq_priority_string(priority_level_t priority) {
    switch (priority) {
        case PRIORITY_LOW:
            return "LOW";
        case PRIORITY_NORMAL:
            return "NORMAL";
        case PRIORITY_HIGH:
            return "HIGH";
        default:
            return "UNKNOWN";
    }
}

/**
 * Get error message string
 */
const char* pq_error_string(int error_code) {
    switch (error_code) {
        case PQ_SUCCESS:
            return "Success";
        case PQ_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case PQ_ERROR_FULL:
            return "Queue full";
        case PQ_ERROR_EMPTY:
            return "Queue empty";
        case PQ_ERROR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}
