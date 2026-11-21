/**
 * resource_queue.cpp
 * 
 * Implementation of Resource Request Queue
 */

#include "resource_queue.h"
#include <algorithm>

// Global instance
ResourceRequestQueue* g_resource_queue = nullptr;

ResourceRequestQueue::ResourceRequestQueue() 
    : next_request_id_(1) {
}

ResourceRequestQueue::~ResourceRequestQueue() {
    // Clean up pending requests
}

uint16_t ResourceRequestQueue::AddRequest(const std::string& url, 
                                         const std::string& method,
                                         ResourcePriority priority,
                                         std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint16_t request_id = next_request_id_++;
    if (next_request_id_ == 0) next_request_id_ = 1;  // Skip 0
    
    ResourceRequest req;
    req.request_id = request_id;
    req.url = url;
    req.method = method;
    req.priority = priority;
    req.timestamp = time(nullptr);
    req.last_activity = req.timestamp;
    req.state = STATE_PENDING;
    req.callback = callback;
    
    requests_[request_id] = req;
    
    return request_id;
}

bool ResourceRequestQueue::GetNextPendingRequest(ResourceRequest** request) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find highest priority pending request
    ResourceRequest* best = nullptr;
    for (auto& pair : requests_) {
        if (pair.second.state == STATE_PENDING) {
            if (!best || pair.second.priority > best->priority) {
                best = &pair.second;
            }
        }
    }
    
    if (best) {
        *request = best;
        return true;
    }
    
    return false;
}

void ResourceRequestQueue::MarkRequestSent(uint16_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = requests_.find(request_id);
    if (it != requests_.end()) {
        it->second.state = STATE_SENT;
        it->second.last_activity = time(nullptr);
    }
}

void ResourceRequestQueue::AddResponseFragment(uint16_t request_id, 
                                              const uint8_t* data, 
                                              size_t size,
                                              bool is_last_fragment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = requests_.find(request_id);
    if (it == requests_.end()) return;
    
    ResourceRequest& req = it->second;
    
    // Append fragment data
    req.response_data.insert(req.response_data.end(), data, data + size);
    req.next_fragment_num++;
    req.last_activity = time(nullptr);
    req.state = STATE_RECEIVING;
    
    if (is_last_fragment) {
        req.final_fragment_received = true;
        req.state = STATE_COMPLETE;
        
        // Invoke callback
        if (req.callback) {
            req.callback(true);
        }
    }
}

ResourceRequest* ResourceRequestQueue::GetRequest(uint16_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = requests_.find(request_id);
    if (it != requests_.end()) {
        return &it->second;
    }
    
    return nullptr;
}

void ResourceRequestQueue::RemoveRequest(uint16_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    requests_.erase(request_id);
}

void ResourceRequestQueue::CheckTimeouts() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    time_t now = time(nullptr);
    
    for (auto& pair : requests_) {
        ResourceRequest& req = pair.second;
        
        // Check request timeout (30 seconds from creation)
        if ((req.state == STATE_PENDING || req.state == STATE_SENT) &&
            (now - req.timestamp) > REQUEST_TIMEOUT_SEC) {
            req.state = STATE_FAILED;
            if (req.callback) {
                req.callback(false);
            }
        }
        
        // Check fragment timeout (5 seconds since last activity)
        if (req.state == STATE_RECEIVING &&
            (now - req.last_activity) > FRAGMENT_TIMEOUT_SEC) {
            req.retry_count++;
            if (req.retry_count >= MAX_RETRIES) {
                req.state = STATE_FAILED;
                if (req.callback) {
                    req.callback(false);
                }
            } else {
                // Retry will be triggered by sender thread
                req.state = STATE_SENT;
            }
        }
    }
}

void ResourceRequestQueue::GetStats(size_t* pending, size_t* active, size_t* completed) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t p = 0, a = 0, c = 0;
    
    for (const auto& pair : requests_) {
        switch (pair.second.state) {
            case STATE_PENDING:
                p++;
                break;
            case STATE_SENT:
            case STATE_RECEIVING:
                a++;
                break;
            case STATE_COMPLETE:
                c++;
                break;
            default:
                break;
        }
    }
    
    if (pending) *pending = p;
    if (active) *active = a;
    if (completed) *completed = c;
}
