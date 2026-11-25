/**
 * http_proxy.h
 * 
 * HTTP Proxy Server for ICMP Browser
 * Listens on localhost:8888 and forwards HTTP requests through ICMP tunnel
 */

#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include "../common/thread_compat.h"

// Forward declaration - ICMP session will be fully defined when integrated
typedef struct icmp_session_s icmp_session_t;

// Constants
#define HTTP_PROXY_DEFAULT_PORT 8888
#define HTTP_PROXY_BIND_ADDRESS "127.0.0.1"
#define HTTP_PROXY_MAX_CONNECTIONS 32
#define HTTP_PROXY_BUFFER_SIZE 65536
#define HTTP_PROXY_TIMEOUT_MS 30000

// Error codes
#define HTTP_PROXY_SUCCESS 0
#define HTTP_PROXY_ERROR_SOCKET -1
#define HTTP_PROXY_ERROR_BIND -2
#define HTTP_PROXY_ERROR_LISTEN -3
#define HTTP_PROXY_ERROR_THREAD -4
#define HTTP_PROXY_ERROR_INVALID_PARAM -5
#define HTTP_PROXY_ERROR_ACCEPT -6
#define HTTP_PROXY_ERROR_RECV -7
#define HTTP_PROXY_ERROR_SEND -8
#define HTTP_PROXY_ERROR_PARSE -9
#define HTTP_PROXY_ERROR_TUNNEL -10

// HTTP request structure
typedef struct {
    char method[16];        // GET, POST, HEAD, etc.
    char uri[2048];         // Request URI
    char version[16];       // HTTP/1.0, HTTP/1.1
    char host[256];         // Host header
    int port;               // Port (default 80)
    int content_length;     // Content-Length header
    int keep_alive;         // Connection: keep-alive
    char *headers;          // Full headers string
    size_t headers_len;
    uint8_t *body;          // Request body (POST data)
    size_t body_len;
} http_request_t;

// HTTP response structure
typedef struct {
    int status_code;        // 200, 404, etc.
    char status_text[64];   // OK, Not Found, etc.
    char version[16];       // HTTP/1.0, HTTP/1.1
    int content_length;     // Content-Length
    int chunked;            // Transfer-Encoding: chunked
    char *headers;          // Full headers string
    size_t headers_len;
    uint8_t *body;          // Response body
    size_t body_len;
} http_response_t;

// Client connection structure
typedef struct http_proxy_s http_proxy_t;  // Forward declaration

typedef struct {
    int socket_fd;          // Client socket
    struct sockaddr_in addr; // Client address
    thread_t thread;        // Handler thread
    int active;             // Connection active
    uint32_t connection_id; // Unique connection ID
    time_t last_activity;   // Last activity timestamp
    http_proxy_t *proxy;    // Pointer to parent proxy
} http_client_t;

// Proxy server structure
struct http_proxy_s {
    int listen_socket;              // Listening socket
    uint16_t port;                  // Listen port
    int running;                    // Server running flag
    thread_t accept_thread;         // Accept thread
    http_client_t *clients;         // Client connections array
    size_t max_clients;             // Maximum concurrent clients
    mutex_t clients_lock;           // Mutex for clients array
    icmp_session_t *icmp_session;   // ICMP tunnel session
    uint32_t next_connection_id;    // Next connection ID
    mutex_t stats_lock;             // Statistics mutex
    uint64_t total_requests;        // Total requests handled
    uint64_t total_bytes_sent;      // Total bytes sent to clients
    uint64_t total_bytes_received;  // Total bytes received from clients
    uint16_t data_sequence_number;    // Current data sequence number for ICMP protocol
    mutex_t data_seq_lock;             // Mutex for data_sequence_number
};

/**
 * Create HTTP proxy server
 * 
 * @param port Port to listen on (default HTTP_PROXY_DEFAULT_PORT)
 * @param icmp_session ICMP tunnel session to use
 * @return Proxy context or NULL on error
 */
http_proxy_t* http_proxy_create(uint16_t port, icmp_session_t *icmp_session);

/**
 * Start proxy server (begins accepting connections)
 * 
 * @param proxy Proxy context
 * @return HTTP_PROXY_SUCCESS on success, error code otherwise
 */
int http_proxy_start(http_proxy_t *proxy);

/**
 * Stop proxy server
 * 
 * @param proxy Proxy context
 */
void http_proxy_stop(http_proxy_t *proxy);

/**
 * Destroy proxy server and free resources
 * 
 * @param proxy Proxy context
 */
void http_proxy_destroy(http_proxy_t *proxy);

/**
 * Parse HTTP request from buffer
 * 
 * @param buffer Raw HTTP request data
 * @param size Buffer size
 * @param request Output HTTP request structure
 * @return HTTP_PROXY_SUCCESS on success, error code otherwise
 */
int http_parse_request(const uint8_t *buffer, size_t size, http_request_t *request);

/**
 * Build HTTP response into buffer
 * 
 * @param response HTTP response structure
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param written Bytes written to buffer
 * @return HTTP_PROXY_SUCCESS on success, error code otherwise
 */
int http_build_response(const http_response_t *response, uint8_t *buffer, 
                        size_t buffer_size, size_t *written);

/**
 * Free HTTP request resources
 * 
 * @param request HTTP request to free
 */
void http_free_request(http_request_t *request);

/**
 * Free HTTP response resources
 * 
 * @param response HTTP response to free
 */
void http_free_response(http_response_t *response);

/**
 * Get proxy statistics
 * 
 * @param proxy Proxy context
 * @param total_requests Total requests handled
 * @param total_bytes_sent Total bytes sent
 * @param total_bytes_received Total bytes received
 * @param active_connections Current active connections
 */
void http_proxy_get_stats(http_proxy_t *proxy, uint64_t *total_requests,
                          uint64_t *total_bytes_sent, uint64_t *total_bytes_received,
                          int *active_connections);

/**
 * Print proxy statistics
 * 
 * @param proxy Proxy context
 */
void http_proxy_print_stats(http_proxy_t *proxy);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PROXY_H */
