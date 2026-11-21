# Requirements Document

## Introduction

This document specifies the requirements for implementing full resource interception in the ICMP Browser, a CEF-based web browser that tunnels HTTP traffic through ICMP packets. The current implementation successfully loads main HTML documents via the ICMP tunnel, but fails to load sub-resources (CSS, JavaScript, images, fonts, etc.) because they attempt direct HTTP connections instead of using the tunnel. This feature will intercept all resource requests made by the CEF browser and route them through the ICMP tunnel, enabling complete website functionality.

## Glossary

- **CEF_Client**: The Windows-based client application using Chromium Embedded Framework
- **ICMP_Tunnel**: The encrypted communication channel using ICMP packets with AES-GCM encryption
- **Resource**: Any web asset requested by the browser (HTML, CSS, JavaScript, images, fonts, etc.)
- **Request_Handler**: CEF component that intercepts resource requests
- **Resource_Queue**: Client-side queue managing multiple concurrent resource requests
- **Request_ID**: Unique identifier matching a client request with its server response
- **Fragment**: A chunk of response data (1200 bytes) transmitted via ICMP
- **HTTP_Proxy**: Server-side component that performs actual HTTP/HTTPS requests
- **Response_Cache**: Server-side storage mapping Request_IDs to cached HTTP responses
- **Session**: Established ICMP tunnel connection with Diffie-Hellman key exchange
- **Priority**: Importance level assigned to resource requests (HTML/CSS high, images low)

## Requirements

### Requirement 1: CEF Resource Request Interception

**User Story:** As the CEF_Client, I want to intercept all resource requests made by the browser, so that they can be routed through the ICMP_Tunnel instead of direct HTTP connections.

#### Acceptance Criteria

1. THE CEF_Client SHALL implement CefResourceRequestHandler to intercept resource requests
2. WHEN a resource request is initiated by the browser, THE CEF_Client SHALL capture the request URL, method, and headers
3. WHEN a resource request is captured, THE CEF_Client SHALL prevent the default HTTP connection attempt
4. THE CEF_Client SHALL return a custom CefResourceHandler that provides data via the ICMP_Tunnel
5. THE Resource_Handler SHALL support all HTTP methods (GET, POST, PUT, DELETE)
6. THE Resource_Handler SHALL preserve HTTP headers from the original request

### Requirement 2: Client-Side Request Queue Management

**User Story:** As the CEF_Client, I want to manage multiple concurrent resource requests, so that the browser can load multiple resources simultaneously without blocking.

#### Acceptance Criteria

1. THE CEF_Client SHALL maintain a Resource_Queue for pending requests
2. WHEN a new resource request is intercepted, THE CEF_Client SHALL generate a unique Request_ID
3. WHEN a Request_ID is generated, THE CEF_Client SHALL add the request to the Resource_Queue with URL, headers, and callback
4. THE Resource_Queue SHALL support concurrent requests (minimum 20 simultaneous requests)
5. WHEN a response arrives from the ICMP_Tunnel, THE CEF_Client SHALL match it to the correct request using the Request_ID
6. WHEN a request completes, THE CEF_Client SHALL remove it from the Resource_Queue and invoke the callback
7. WHEN a request times out (30 seconds), THE CEF_Client SHALL remove it from the queue and return an error

### Requirement 3: Request Priority Management

**User Story:** As the CEF_Client, I want to prioritize critical resources, so that HTML and CSS load before images and other non-essential resources.

#### Acceptance Criteria

1. THE CEF_Client SHALL assign a Priority to each resource request based on resource type
2. WHEN a resource is HTML or CSS, THE CEF_Client SHALL assign high Priority
3. WHEN a resource is JavaScript, THE CEF_Client SHALL assign medium Priority
4. WHEN a resource is an image, font, or other media, THE CEF_Client SHALL assign low Priority
5. WHEN multiple requests are queued, THE CEF_Client SHALL transmit high Priority requests before low Priority requests
6. THE Priority SHALL be determined by file extension and MIME type

### Requirement 4: ICMP Request Transmission Protocol

**User Story:** As the CEF_Client, I want to transmit resource requests through the ICMP_Tunnel with unique identifiers, so that the HTTP_Proxy can process them and return the correct responses.

#### Acceptance Criteria

1. WHEN transmitting a request, THE CEF_Client SHALL include the Request_ID in the ICMP packet payload
2. THE ICMP packet payload SHALL contain Request_ID (2 bytes), URL length (2 bytes), and URL string
3. WHEN the URL exceeds 1000 bytes, THE CEF_Client SHALL split it across multiple ICMP packets
4. THE CEF_Client SHALL encrypt all request data using AES-GCM with the Session key
5. WHEN transmission fails, THE CEF_Client SHALL retry up to 3 times with exponential backoff
6. THE CEF_Client SHALL use the existing icmp_send_reliable function for packet transmission

### Requirement 5: Server-Side Multi-Request Support

**User Story:** As the HTTP_Proxy, I want to handle multiple concurrent HTTP requests, so that the CEF_Client can load multiple resources simultaneously.

#### Acceptance Criteria

1. THE HTTP_Proxy SHALL maintain a Response_Cache supporting multiple concurrent responses
2. WHEN a request packet arrives, THE HTTP_Proxy SHALL extract the Request_ID and URL
3. WHEN a Request_ID is extracted, THE HTTP_Proxy SHALL create a new cache entry for that Request_ID
4. THE Response_Cache SHALL support at least 50 concurrent cached responses
5. WHEN the cache is full, THE HTTP_Proxy SHALL evict the oldest entry
6. THE HTTP_Proxy SHALL perform the HTTP request asynchronously without blocking other requests
7. WHEN an HTTP request completes, THE HTTP_Proxy SHALL store the complete response in the Response_Cache indexed by Request_ID

### Requirement 6: Chunked Response Transmission

**User Story:** As the HTTP_Proxy, I want to transmit large HTTP responses in fragments, so that they can be sent through the ICMP_Tunnel efficiently.

#### Acceptance Criteria

1. WHEN an HTTP response exceeds 1200 bytes, THE HTTP_Proxy SHALL split it into Fragments
2. THE Fragment size SHALL be 1200 bytes maximum
3. WHEN the CEF_Client requests a Fragment, THE ICMP packet SHALL include Request_ID and fragment number
4. WHEN a Fragment request arrives, THE HTTP_Proxy SHALL retrieve the cached response by Request_ID
5. THE HTTP_Proxy SHALL extract the requested Fragment by offset (fragment_num × 1200)
6. THE HTTP_Proxy SHALL return the Fragment data with Request_ID and fragment_num in the response packet
7. WHEN all Fragments are transmitted, THE HTTP_Proxy SHALL keep the cached response for 60 seconds before eviction

### Requirement 7: Client-Side Response Assembly

**User Story:** As the CEF_Client, I want to receive and assemble response fragments, so that I can provide complete resource data to the browser.

#### Acceptance Criteria

1. WHEN a Fragment response arrives, THE CEF_Client SHALL match it to the correct request using Request_ID
2. THE CEF_Client SHALL append the Fragment data to the request's response buffer
3. WHEN a Fragment is smaller than 1200 bytes, THE CEF_Client SHALL treat it as the final Fragment
4. WHEN the final Fragment is received, THE CEF_Client SHALL mark the request as complete
5. THE CEF_Client SHALL provide the complete response data to the CefResourceHandler callback
6. THE CEF_Client SHALL parse HTTP headers from the response to provide Content-Type to CEF
7. WHEN Fragment reception times out (5 seconds), THE CEF_Client SHALL request the Fragment again up to 3 times

### Requirement 8: Request-Response Protocol Extension

**User Story:** As the ICMP_Tunnel, I want to support extended packet types for resource requests and responses, so that the existing protocol can handle multiple concurrent requests.

#### Acceptance Criteria

1. THE ICMP_Tunnel SHALL define a new packet type RESOURCE_REQUEST (0x05)
2. THE ICMP_Tunnel SHALL define a new packet type RESOURCE_RESPONSE (0x06)
3. WHEN a RESOURCE_REQUEST packet is received, THE HTTP_Proxy SHALL parse Request_ID and URL
4. WHEN a RESOURCE_RESPONSE packet is sent, THE HTTP_Proxy SHALL include Request_ID, fragment_num, and Fragment data
5. THE packet format SHALL maintain backward compatibility with existing DATA packets (0x03)
6. THE protocol SHALL use the existing sequence numbering system
7. THE protocol SHALL use the existing AES-GCM encryption for all packet payloads

### Requirement 9: Error Handling and Timeout Management

**User Story:** As the CEF_Client, I want to handle request failures gracefully, so that the browser can display error pages instead of hanging indefinitely.

#### Acceptance Criteria

1. WHEN a request times out after 30 seconds, THE CEF_Client SHALL return an HTTP 504 Gateway Timeout error
2. WHEN the ICMP_Tunnel connection is lost, THE CEF_Client SHALL fail all pending requests with HTTP 503 Service Unavailable
3. WHEN the HTTP_Proxy cannot reach the destination server, THE HTTP_Proxy SHALL return an HTTP 502 Bad Gateway error
4. WHEN an HTTP request returns an error status (4xx, 5xx), THE HTTP_Proxy SHALL transmit the error response to the CEF_Client
5. THE CEF_Client SHALL provide error responses to CEF so the browser can display error pages
6. WHEN a Fragment is lost, THE CEF_Client SHALL request it again after a 1-second timeout
7. WHEN Fragment retries are exhausted (3 attempts), THE CEF_Client SHALL fail the request with HTTP 504

### Requirement 10: Session State Management

**User Story:** As the ICMP_Tunnel, I want to maintain session state across multiple requests, so that the encryption keys and connection remain valid.

#### Acceptance Criteria

1. THE Session SHALL remain active for the duration of multiple resource requests
2. WHEN a handshake completes, THE Session key SHALL be used for all subsequent resource requests
3. THE Session SHALL support interleaved requests (request A sent, then request B sent, then response A received)
4. WHEN a Session is terminated, THE CEF_Client SHALL fail all pending requests
5. THE CEF_Client SHALL perform a new handshake when the Session expires (300 seconds of inactivity)
6. THE HTTP_Proxy SHALL maintain Session state including Response_Cache entries per Session
7. WHEN a Session expires on the server, THE HTTP_Proxy SHALL clear all cached responses for that Session

### Requirement 11: Performance and Resource Limits

**User Story:** As the system, I want to enforce resource limits, so that memory usage remains bounded and performance is acceptable.

#### Acceptance Criteria

1. THE Resource_Queue SHALL limit concurrent requests to 20 maximum
2. WHEN the Resource_Queue is full, THE CEF_Client SHALL block new requests until slots are available
3. THE Response_Cache SHALL limit cached responses to 50 maximum
4. THE Response_Cache SHALL limit individual response size to 10 MB maximum
5. WHEN a response exceeds 10 MB, THE HTTP_Proxy SHALL truncate it and return an error header
6. THE CEF_Client SHALL limit Fragment request rate to 100 fragments per second per request
7. THE Fragment size SHALL remain 1200 bytes to fit within ICMP packet constraints

### Requirement 12: Testing and Validation Requirements

**User Story:** As a developer, I want to validate resource interception functionality, so that I can verify complete websites load correctly.

#### Acceptance Criteria

1. THE system SHALL successfully load example.com with all resources (HTML only)
2. THE system SHALL successfully load google.com with all resources (HTML, CSS, JS)
3. THE system SHALL successfully load wikipedia.org with all resources including images
4. THE system SHALL handle redirects (3xx responses) by following them automatically
5. THE system SHALL preserve cookies across multiple requests
6. THE system SHALL support HTTPS URLs by using the existing HTTP proxy HTTPS support
7. WHEN loading a page, THE system SHALL display loading progress in the UI
