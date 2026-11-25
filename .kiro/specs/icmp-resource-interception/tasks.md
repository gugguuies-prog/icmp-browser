# Implementation Plan: ICMP Resource Interception

## Overview

This implementation extends the ICMP Browser to intercept all CEF resource requests (CSS, JavaScript, images, fonts) and route them through the ICMP tunnel. The implementation adds request queuing with priorities, multi-slot response caching, and concurrent HTTP fetching to enable complete website functionality through ICMP.

## Tasks

- [x] 1. Extend protocol with resource request packet types
  - Create RESOURCE_REQUEST (0x05) and RESOURCE_RESPONSE (0x06) packet type definitions
  - Define payload structures in src/common/proto.h
  - Add request_id field (2 bytes) to packet payloads
  - _Requirements: 8.1, 8.2, 8.3, 8.4_

- [x] 2. Implement server-side multi-response cache
  - [x] 2.1 Create multi-cache data structures
    - Create src/server/multi_cache.h and multi_cache.c files
    - Define cache_entry_t structure with request_id, response_data, size, timestamp
    - Define multi_response_cache_t structure with 50 entry array and mutex
    - _Requirements: 5.1, 5.2, 5.3, 5.4_
  
  - [x] 2.2 Implement cache operations
    - Implement cache_init(), cache_store(), cache_get() functions
    - Implement cache_get_fragment() for fragment retrieval
    - Implement cache_evict_oldest() for LRU eviction
    - Implement cache_cleanup_expired() for 60-second expiration
    - Add pthread_mutex protection for thread safety
    - _Requirements: 5.5, 5.6, 5.7_
  
  - [ ]* 2.3 Write unit tests for multi-cache
    - Test store and retrieve by request ID
    - Test fragment retrieval with multiple fragments
    - Test LRU eviction when cache is full
    - Test expiration cleanup
    - _Requirements: 5.1, 5.4, 5.5_

- [ ] 3. Modify server ICMP handler for resource requests
  - [ ] 3.1 Update session structure
    - Remove old single-cache fields from icmp_session_info_t in src/server/icmp_handler.h
    - Add multi_response_cache_t pointer to icmp_session_info_t
    - Initialize cache on session creation
    - Free cache on session cleanup
    - _Requirements: 5.1, 10.6, 10.7_
  
  - [ ] 3.2 Implement RESOURCE_REQUEST packet handler
    - Add packet type check for PROTO_TYPE_RESOURCE_REQUEST in src/server/icmp_handler.c
    - Parse request_id, url_length, and url from packet payload
    - Check if request_id exists in cache
    - If not cached and fragment_num=0, trigger HTTP fetch
    - Store HTTP response in cache with request_id
    - _Requirements: 5.2, 5.3, 5.7, 8.3_
  
  - [ ] 3.3 Implement fragment response logic
    - Retrieve fragment from cache using cache_get_fragment()
    - Build RESOURCE_RESPONSE payload with request_id and fragment data
    - Encapsulate and send via icmp_send_reply()
    - Set packet type to PROTO_TYPE_RESOURCE_RESPONSE
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 8.4_
  
  - [ ]* 3.4 Write integration tests for request handler
    - Test receiving RESOURCE_REQUEST packet
    - Test HTTP fetch and cache storage
    - Test fragment response generation
    - Test multiple concurrent requests
    - _Requirements: 5.6, 6.7, 8.3, 8.4_

- [ ] 4. Checkpoint - Verify server-side functionality
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 5. Extend HTTP proxy for URL fetching
  - [ ] 5.1 Create http_proxy_fetch() function
    - Add http_proxy_fetch() function in src/server/http_proxy.c
    - Accept URL string, response buffer, response size parameters
    - Use libcurl to fetch URL with 30-second timeout
    - Support HTTPS URLs and follow redirects automatically
    - Return response body and size
    - _Requirements: 12.6_
  
  - [ ] 5.2 Add error handling for HTTP failures
    - Handle DNS resolution failures (return empty response)
    - Handle connection timeouts (return HTTP 504)
    - Handle HTTP errors 4xx/5xx (cache and return error response)
    - Log all errors to stderr
    - _Requirements: 9.3, 9.4_
  
  - [ ]* 5.3 Write unit tests for http_proxy_fetch
    - Test fetching valid URL (http://example.com)
    - Test handling redirects
    - Test timeout after 30 seconds
    - Test invalid URL error handling
    - _Requirements: 9.3_

- [ ] 6. Implement client-side request queue
  - [ ] 6.1 Create ResourceRequestQueue class
    - Create src/browser/resource_queue.h and resource_queue.cpp files
    - Define ResourceRequest structure with request_id, url, method, headers, priority, timestamp, state, response_data, callback
    - Implement ResourceRequestQueue class with AddRequest(), GetNextPendingRequest(), MarkRequestSent(), AddResponseFragment(), GetRequest(), RemoveRequest() methods
    - Add std::mutex for thread safety
    - Add next_request_id_ counter with wraparound (0-65535)
    - _Requirements: 2.1, 2.2, 2.3, 2.4_
  
  - [ ] 6.2 Implement priority management
    - Assign priority 3 for HTML/CSS (check file extension .html, .htm, .css)
    - Assign priority 2 for JavaScript (check .js extension)
    - Assign priority 1 for images/fonts (check .jpg, .png, .gif, .svg, .woff, .ttf)
    - Implement GetNextPendingRequest() to return highest priority request first
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_
  
  - [ ] 6.3 Implement fragment assembly logic
    - In AddResponseFragment(), append fragment data to response_data vector
    - Track next_fragment_num (increment after each fragment)
    - Detect last fragment (size < 1200 bytes)
    - Set state to COMPLETE when last fragment received
    - Invoke callback with success=true
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_
  
  - [ ] 6.4 Implement timeout detection
    - Add CheckTimeouts() method that iterates all requests
    - Check if (current_time - timestamp) > 30 seconds for PENDING/SENT requests
    - Check if (current_time - last_activity) > 5 seconds for RECEIVING requests
    - Mark timed-out requests as FAILED
    - Invoke callback with success=false
    - _Requirements: 2.7, 7.7, 9.1_
  
  - [ ]* 6.5 Write unit tests for ResourceRequestQueue
    - Test AddRequest generates sequential IDs
    - Test priority ordering (high priority retrieved first)
    - Test fragment assembly (3 fragments combine correctly)
    - Test timeout detection after 30 seconds
    - Test concurrent access with multiple threads
    - _Requirements: 2.1, 2.2, 2.4, 2.7, 3.5_

- [ ] 7. Implement CEF resource request interception
  - [ ] 7.1 Create ICMPResourceRequestHandler class
    - Create src/browser/icmp_resource_handler.h and icmp_resource_handler.cpp files
    - Inherit from CefResourceRequestHandler
    - Implement GetResourceHandler() method
    - Check if request URL starts with http:// or https://
    - Return new ICMPResourceHandler instance for HTTP/HTTPS requests
    - _Requirements: 1.1, 1.2, 1.3, 1.4_
  
  - [ ] 7.2 Create ICMPResourceHandler class
    - Define ICMPResourceHandler class inheriting from CefResourceHandler
    - Store url_, method_, request_id_, response_data_, read_offset_, completed_ members
    - Implement constructor that stores URL and method
    - _Requirements: 1.4, 1.5, 1.6_
  
  - [ ] 7.3 Implement Open() method
    - Generate request_id by calling g_resource_queue.AddRequest()
    - Determine priority based on URL file extension
    - Pass callback lambda that sets completed_=true and stores response_data_
    - Set handle_request=true to indicate asynchronous handling
    - Invoke callback->Continue() when response completes
    - _Requirements: 2.2, 2.3, 2.6, 3.1_
  
  - [ ] 7.4 Implement Read() method
    - Check if response is complete (completed_==true)
    - Copy data from response_data_ starting at read_offset_
    - Update read_offset_ by bytes copied
    - Set bytes_read to actual bytes copied
    - Return true if more data available, false if end reached
    - _Requirements: 7.5_
  
  - [ ] 7.5 Implement GetResponseHeaders() method
    - Parse HTTP headers from response_data_ (first line until \r\n\r\n)
    - Extract Content-Type header
    - Set response->SetMimeType() with Content-Type
    - Set response_length to total response_data_.size()
    - _Requirements: 7.6_
  
  - [ ] 7.6 Implement Cancel() method
    - Remove request from g_resource_queue by request_id_
    - Set completed_=true to stop any pending operations
    - _Requirements: 9.5_
  
  - [ ]* 7.7 Write unit tests for ICMPResourceHandler
    - Test Open() adds request to queue
    - Test Read() returns data after completion
    - Test Cancel() removes request from queue
    - Test GetResponseHeaders() parses Content-Type
    - _Requirements: 1.1, 1.2, 1.4, 7.5, 7.6_

- [ ] 8. Integrate resource handler into CEF client
  - Modify src/browser/cef_browser.cpp to register ICMPResourceRequestHandler
  - In CefBrowserClient::GetResourceRequestHandler(), return ICMPResourceRequestHandler instance
  - Create global g_resource_queue instance
  - Initialize g_resource_queue on browser startup
  - _Requirements: 1.1, 1.2_

- [ ] 9. Implement client resource request sender
  - [ ] 9.1 Create SendResourceRequest() function
    - Add SendResourceRequest() function in src/browser/cef_browser.cpp
    - Build resource_request_payload_t with request_id (htons), url_length (htons), url
    - Encapsulate with proto_encapsulate() using PROTO_TYPE_RESOURCE_REQUEST
    - Set fragment_num in packet header
    - Send via icmp_send_reliable()
    - Handle reply and return result
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6_
  
  - [ ] 9.2 Create resource request sender thread
    - Create ResourceRequestSenderThread() function
    - Loop while session is connected
    - Call g_resource_queue.GetNextPendingRequest() to get highest priority request
    - Call SendResourceRequest() with request_id, url, fragment_num=0
    - Call g_resource_queue.MarkRequestSent() after sending
    - Call g_resource_queue.CheckTimeouts() every iteration
    - Sleep 10ms between requests for rate limiting
    - _Requirements: 2.4, 2.5, 2.6, 3.5, 4.5, 11.6_
  
  - [ ] 9.3 Start sender thread on session connect
    - Create thread in session connection code
    - Store thread handle for cleanup
    - Signal thread to stop on session disconnect
    - _Requirements: 10.1, 10.2, 10.3_

- [ ] 10. Implement client resource response receiver
  - [ ] 10.1 Add RESOURCE_RESPONSE packet handler
    - In existing ICMP receive loop, add packet type check for PROTO_TYPE_RESOURCE_RESPONSE
    - Parse resource_response_payload_t from decrypted data
    - Extract request_id (ntohs), fragment_num, data
    - _Requirements: 7.1, 8.4_
  
  - [ ] 10.2 Add fragment to queue
    - Call g_resource_queue.AddResponseFragment() with request_id, data, size
    - Determine if last fragment (size < 1200)
    - Pass is_last_fragment flag to AddResponseFragment()
    - _Requirements: 7.2, 7.3, 7.4_
  
  - [ ] 10.3 Request next fragment if not last
    - If not last fragment, retrieve request from queue
    - Call SendResourceRequest() with request_id, url, fragment_num+1
    - Update request's next_fragment_num
    - _Requirements: 6.5, 6.6_
  
  - [ ]* 10.4 Write integration tests for request/response flow
    - Test sending RESOURCE_REQUEST packet
    - Test receiving RESOURCE_RESPONSE packet
    - Test multi-fragment assembly (3 fragments)
    - Test fragment retry on timeout
    - _Requirements: 4.1, 4.4, 7.1, 7.7_

- [ ] 11. Checkpoint - Verify client-side functionality
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 12. Implement error handling and recovery
  - [ ] 12.1 Handle request timeouts
    - In CheckTimeouts(), mark PENDING/SENT requests timed out after 30 seconds
    - Set state to FAILED, set error code to ERROR_TIMEOUT
    - Invoke callback with success=false
    - In ICMPResourceHandler::GetResponseHeaders(), return HTTP 504 for timeouts
    - _Requirements: 9.1_
  
  - [ ] 12.2 Handle fragment timeouts
    - In CheckTimeouts(), check RECEIVING requests for fragment timeout (5 seconds since last_activity)
    - Increment retry_count, re-send fragment request if retry_count < 3
    - After 3 retries, mark as FAILED with ERROR_FRAGMENT_LOST
    - _Requirements: 7.7, 9.7_
  
  - [ ] 12.3 Handle session disconnection
    - In session disconnect handler, iterate all requests in queue
    - Mark all PENDING/SENT/RECEIVING requests as FAILED with ERROR_SESSION_LOST
    - Invoke callbacks with success=false
    - Return HTTP 503 to CEF for session lost errors
    - _Requirements: 9.2, 10.4_
  
  - [ ] 12.4 Handle server HTTP errors
    - Server should cache HTTP error responses (4xx, 5xx) normally
    - Client receives error response via normal fragment mechanism
    - CEF displays error page automatically
    - _Requirements: 9.4_

- [ ] 13. Add resource loading metrics and logging
  - Add debug logging for request creation (request_id, url, priority)
  - Add debug logging for fragment transmission (request_id, fragment_num)
  - Add debug logging for response completion (request_id, total_size, duration)
  - Add metrics collection (requests/sec, fragments/sec, avg response time)
  - Output metrics to debug log every 10 seconds
  - _Requirements: 12.7_

- [ ] 14. Implement performance optimizations
  - [ ] 14.1 Optimize priority queue retrieval
    - Replace linear search with heap-based priority queue (std::priority_queue)
    - Compare by priority first, then by timestamp (FIFO within priority)
    - _Requirements: 3.5, 11.1_
  
  - [ ] 14.2 Add request rate limiting
    - Enforce maximum 100 fragments per second per request
    - Track fragments sent per request in last second
    - Sleep if limit exceeded
    - _Requirements: 11.6_
  
  - [ ] 14.3 Enforce concurrent request limit
    - Track count of SENT and RECEIVING requests
    - Block AddRequest() if count >= 20
    - Wait on condition variable until slot available
    - _Requirements: 11.1, 11.2_

- [ ] 15. Final checkpoint - Integration testing
  - Ensure all tests pass, ask the user if questions arise.

- [ ]* 16. End-to-end validation
  - [ ]* 16.1 Test loading example.com
    - Start client and server
    - Navigate to http://example.com
    - Verify HTML loads and renders
    - Check debug logs for RESOURCE_REQUEST packets
    - _Requirements: 12.1_
  
  - [ ]* 16.2 Test loading google.com
    - Navigate to http://google.com
    - Verify HTML, CSS, JavaScript all load
    - Verify search functionality works
    - Check that multiple resources load concurrently
    - _Requirements: 12.2_
  
  - [ ]* 16.3 Test loading wikipedia.org
    - Navigate to http://wikipedia.org
    - Verify images display correctly
    - Verify CSS styling applied
    - Verify links work
    - _Requirements: 12.3_
  
  - [ ]* 16.4 Test redirect handling
    - Navigate to http://google.com (redirects to https)
    - Verify redirect followed automatically
    - Verify final page loads correctly
    - _Requirements: 12.4_
  
  - [ ]* 16.5 Test error page display
    - Navigate to http://nonexistent.invalid
    - Verify error page displays (not hang)
    - Check logs for HTTP fetch failure
    - _Requirements: 9.3_
  
  - [ ]* 16.6 Test large resource loading
    - Navigate to page with large image (>1 MB)
    - Verify image loads and displays correctly
    - Check logs for fragment count (should be >800)
    - _Requirements: 11.4_

- [ ] 17. Documentation and cleanup
  - Update README.md with resource interception feature description
  - Document configuration parameters (max concurrent requests, timeouts)
  - Add code comments for public APIs
  - Remove debug logging that is too verbose

## Notes

- Tasks marked with `*` are optional testing tasks and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation at key milestones
- Implementation uses C++ for client (CEF integration) and C for server (ICMP/HTTP)
- Priority ordering ensures critical resources (CSS/JS) load before images
- Multi-slot cache enables concurrent resource loading for modern websites
- Fragment retry logic handles packet loss gracefully
- End-to-end tests validate real-world websites (Google, Wikipedia)

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1"] },
    { "id": 1, "tasks": ["2.1"] },
    { "id": 2, "tasks": ["2.2"] },
    { "id": 3, "tasks": ["2.3", "3.1"] },
    { "id": 4, "tasks": ["3.2"] },
    { "id": 5, "tasks": ["3.3"] },
    { "id": 6, "tasks": ["3.4", "5.1"] },
    { "id": 7, "tasks": ["4"] },
    { "id": 8, "tasks": ["5.2"] },
    { "id": 9, "tasks": ["5.3", "6.1"] },
    { "id": 10, "tasks": ["6.2"] },
    { "id": 11, "tasks": ["6.3"] },
    { "id": 12, "tasks": ["6.4"] },
    { "id": 13, "tasks": ["6.5", "7.1"] },
    { "id": 14, "tasks": ["7.2"] },
    { "id": 15, "tasks": ["7.3"] },
    { "id": 16, "tasks": ["7.4", "7.5"] },
    { "id": 17, "tasks": ["7.6"] },
    { "id": 18, "tasks": ["7.7", "8"] },
    { "id": 19, "tasks": ["9.1"] },
    { "id": 20, "tasks": ["9.2"] },
    { "id": 21, "tasks": ["9.3", "10.1"] },
    { "id": 22, "tasks": ["10.2"] },
    { "id": 23, "tasks": ["10.3"] },
    { "id": 24, "tasks": ["10.4"] },
    { "id": 25, "tasks": ["11"] },
    { "id": 26, "tasks": ["12.1", "12.2", "12.3", "12.4"] },
    { "id": 27, "tasks": ["13", "14.1", "14.2", "14.3"] },
    { "id": 28, "tasks": ["15"] },
    { "id": 29, "tasks": ["16.1", "16.2", "16.3", "16.4", "16.5", "16.6"] },
    { "id": 30, "tasks": ["17"] }
  ]
}
```
