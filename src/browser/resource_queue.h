/**
 * resource_queue.h
 * 
 * Client-side Resource Request Queue
 * Manages multiple concurrent resource requests with priorities
 */

#ifndef RESOURCE_QUEUE_H
#define RESOURCE_QUEUE_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <ctime>

// Request priority levels
enum ResourcePriority {
    PRIORITY_LOW = 1,      // Images, fonts, media
    PRIORITY_MEDIUM = 2,   // JavaScript
    PRIORITY_HIGH = 3      // HTML, CSS
};

// Request state
enum ResourceState {
    STATE_PENDING,         // Not yet sent
    STATE_SENT,            // Request sent, waiting for first fragment
    STATE_RECEIVING,       // Receiving fragments
    STATE_COMPLETE,        // All fragments received
    STATE_FAILED,          // Timeout or error
    STATE_CANCELLED        // User cancelled
};

/**
 * Resource Request structure
 */
struct ResourceRequest {
    // Identification
    uint16_t request_id;                      // Unique ID (0-65535)
    
    // Request details
    std::string url;                          // Full URL
    std::string method;                       // GET, POST, etc.
    std::map<std::string, std::string> headers;  // HTTP headers
    
    // Priority and timing
    ResourcePriority priority;                // Priority level
    time_t timestamp;                         // Request creation time
    time_t last_activity;                     // Last fragment received time
    
    // State
    ResourceState state;                      // Current state
    
    // Response assembly
    std::vector<uint8_t> response_data;       // Accumulated fragments
    uint8_t next_fragment_num;                // Next expected fragment
    bool final_fragment_received;             // Last fragment flag
    
    // Error tracking
    int retry_count;                          // Number of retries
    
    // Callback
    std::function<void(bool success)> callback;  // Completion callback
    
    ResourceRequest() 
        : request_id(0), priority(PRIORITY_MEDIUM), timestamp(0), 
          last_activity(0), state(STATE_PENDING), next_fragment_num(0),
          final_fragment_received(false), retry_count(0) {}
};

/**
 * Resource Request Queue
 * Thread-safe queue managing multiple concurrent requests
 */
class ResourceRequestQueue {
public:
    ResourceRequestQueue();
    ~ResourceRequestQueue();
    
    /**
     * Add a request to the queue
     * 
     * @param url Resource URL
     * @param method HTTP method (GET, POST, etc.)
     * @param priority Request priority
     * @param callback Completion callback (called with success=true/false)
     * @return Assigned request ID
     */
    uint16_t AddRequest(const std::string& url, 
                       const std::string& method,
                       ResourcePriority priority,
                       std::function<void(bool)> callback);
    
    /**
     * Get next request to send (highest priority)
     * 
     * @param request Output: pointer to request (valid until next call)
     * @return true if request available, false if queue empty
     */
    bool GetNextPendingRequest(ResourceRequest** request);
    
    /**
     * Mark request as sent (waiting for response)
     * 
     * @param request_id Request ID
     */
    void MarkRequestSent(uint16_t request_id);
    
    /**
     * Add response fragment to request
     * 
     * @param request_id Request ID
     * @param data Fragment data
     * @param size Fragment size
     * @param is_last_fragment True if this is the last fragment
     */
    void AddResponseFragment(uint16_t request_id, 
                            const uint8_t* data, 
                            size_t size,
                            bool is_last_fragment);
    
    /**
     * Get request by ID
     * 
     * @param request_id Request ID
     * @return Pointer to request, or nullptr if not found
     */
    ResourceRequest* GetRequest(uint16_t request_id);
    
    /**
     * Remove completed/failed request
     * 
     * @param request_id Request ID
     */
    void RemoveRequest(uint16_t request_id);
    
    /**
     * Check for timed-out requests
     * Should be called periodically (e.g., every second)
     */
    void CheckTimeouts();
    
    /**
     * Get queue statistics
     * 
     * @param pending Output: number of pending requests
     * @param active Output: number of active (sent/receiving) requests
     * @param completed Output: number of completed requests
     */
    void GetStats(size_t* pending, size_t* active, size_t* completed);
    
private:
    std::map<uint16_t, ResourceRequest> requests_;  // Request map
    std::mutex mutex_;                               // Thread safety
    uint16_t next_request_id_;                       // Next ID to assign
    
    // Configuration
    static const int REQUEST_TIMEOUT_SEC = 30;       // Request timeout
    static const int FRAGMENT_TIMEOUT_SEC = 5;       // Fragment timeout
    static const int MAX_RETRIES = 3;                // Max retry count
};

// Global instance
extern ResourceRequestQueue* g_resource_queue;

#endif // RESOURCE_QUEUE_H
