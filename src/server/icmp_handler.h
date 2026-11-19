/**
 * icmp_handler.h
 * 
 * ICMP Handler for Linux server (using raw sockets)
 * 
 * Features:
 * - Raw socket creation (requires root/CAP_NET_RAW)
 * - ICMP Echo Request/Reply handling
 * - Multi-client session management
 * - Non-blocking I/O with select()
 * - Thread-safe operations
 */

#ifndef ICMP_HANDLER_H
#define ICMP_HANDLER_H

#ifndef __linux__
#error "This ICMP handler is for Linux only. Use icmp_engine.h for Windows."
#endif

#include <stdint.h>
#include <stddef.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../common/thread_compat.h"

// Constants
#define ICMP_MTU 1472                    // Maximum payload size (1500 - 20 IP - 8 ICMP)
#define ICMP_RECV_BUFFER_SIZE 2048       // Buffer for incoming packets
#define ICMP_MAX_SESSIONS 256            // Maximum concurrent clients
#define ICMP_SESSION_TIMEOUT_SEC 300     // Session timeout (5 minutes)

// Error codes
#define ICMP_SUCCESS 0
#define ICMP_ERROR_INVALID_PARAM -1
#define ICMP_ERROR_SOCKET_CREATE -2
#define ICMP_ERROR_SOCKET_BIND -3
#define ICMP_ERROR_SEND_FAILED -4
#define ICMP_ERROR_RECV_FAILED -5
#define ICMP_ERROR_TIMEOUT -6
#define ICMP_ERROR_MEMORY -7
#define ICMP_ERROR_SESSION_FULL -8
#define ICMP_ERROR_SESSION_NOT_FOUND -9
#define ICMP_ERROR_PERMISSION -10

// ICMP types
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_ECHO_REQUEST 8

/**
 * ICMP header structure (RFC 792)
 */
typedef struct {
    uint8_t type;                        // ICMP type (0=Echo Reply, 8=Echo Request)
    uint8_t code;                        // ICMP code (0 for Echo)
    uint16_t checksum;                   // Checksum
    uint16_t id;                         // Identifier (session ID)
    uint16_t sequence;                   // Sequence number
} __attribute__((packed)) icmp_header_t;

/**
 * Client session structure
 * Tracks state for each connected client
 */
typedef struct {
    struct sockaddr_in client_addr;      // Client IP address
    uint16_t session_id;                 // Session ID (from ICMP id field)
    uint16_t last_sequence;              // Last received sequence number
    uint16_t data_sequence_counter;      // Counter for DATA packets (starts at 0)
    time_t last_seen;                    // Last activity timestamp
    uint8_t active;                      // Session active flag
    
    // Crypto state
    uint8_t session_key[32];             // AES-256 session key
    uint8_t handshake_complete;          // Handshake completed flag
    
    // Statistics
    uint32_t packets_received;           // Total packets received
    uint32_t packets_sent;               // Total packets sent
    uint32_t bytes_received;             // Total bytes received
    uint32_t bytes_sent;                 // Total bytes sent
    
} icmp_session_info_t;

/**
 * ICMP server context
 * Manages the server state and all client sessions
 */
typedef struct {
    int raw_socket;                      // Raw socket file descriptor
    uint8_t running;                     // Server running flag
    
    // Session table
    icmp_session_info_t sessions[ICMP_MAX_SESSIONS];
    mutex_t sessions_lock;           // Lock for session table
    
    // Receive buffer
    uint8_t recv_buffer[ICMP_RECV_BUFFER_SIZE];
    
    // Statistics
    uint32_t total_packets_received;
    uint32_t total_packets_sent;
    uint64_t total_bytes_received;
    uint64_t total_bytes_sent;
    
    // Callbacks
    void (*on_packet_received)(const struct sockaddr_in *client_addr, 
                                const uint8_t *payload, size_t payload_size,
                                uint16_t session_id, uint16_t sequence);
    
} icmp_server_t;

/**
 * Create ICMP server
 * 
 * @return Pointer to icmp_server_t on success, NULL on error
 * 
 * Note: Requires root privileges or CAP_NET_RAW capability
 * 
 * Example:
 *   icmp_server_t *server = icmp_server_create();
 *   if (server) {
 *       // Use server
 *       icmp_server_destroy(server);
 *   }
 */
icmp_server_t* icmp_server_create(void);

/**
 * Start ICMP server (blocking)
 * 
 * @param server ICMP server context
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Blocks until icmp_server_stop() is called
 *       Handles incoming ICMP Echo Requests and sends Echo Replies
 * 
 * Example:
 *   icmp_server_t *server = icmp_server_create();
 *   icmp_server_start(server);  // Blocking
 */
int icmp_server_start(icmp_server_t *server);

/**
 * Stop ICMP server
 * 
 * @param server ICMP server context
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Signals the server to stop, unblocking icmp_server_start()
 */
int icmp_server_stop(icmp_server_t *server);

/**
 * Send ICMP Echo Reply to client
 * 
 * @param server ICMP server context
 * @param client_addr Client IP address
 * @param session_id Session ID
 * @param sequence Sequence number
 * @param payload Payload data
 * @param payload_size Size of payload
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Automatically calculates ICMP checksum
 * 
 * Example:
 *   uint8_t data[100] = { 0x01, 0x02, 0x03 };
 *   icmp_send_reply(server, &client_addr, 0x1234, 42, data, 100);
 */
int icmp_send_reply(icmp_server_t *server, const struct sockaddr_in *client_addr,
                     uint16_t session_id, uint16_t sequence,
                     const uint8_t *payload, size_t payload_size);

/**
 * Set packet received callback
 * 
 * @param server ICMP server context
 * @param callback Callback function called when packet is received
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Callback signature:
 *   void callback(const struct sockaddr_in *client_addr,
 *                 const uint8_t *payload, size_t payload_size,
 *                 uint16_t session_id, uint16_t sequence);
 * 
 * Example:
 *   void on_packet(const struct sockaddr_in *addr, const uint8_t *data,
 *                  size_t size, uint16_t sid, uint16_t seq) {
 *       printf("Received %zu bytes from client\n", size);
 *   }
 *   icmp_set_packet_callback(server, on_packet);
 */
int icmp_set_packet_callback(icmp_server_t *server,
                               void (*callback)(const struct sockaddr_in*, 
                                                const uint8_t*, size_t,
                                                uint16_t, uint16_t));

/**
 * Get session information
 * 
 * @param server ICMP server context
 * @param session_id Session ID to query
 * @param session_info Output: session information
 * 
 * @return ICMP_SUCCESS on success, ICMP_ERROR_SESSION_NOT_FOUND if not found
 * 
 * Example:
 *   icmp_session_info_t info;
 *   if (icmp_get_session(server, 0x1234, &info) == ICMP_SUCCESS) {
 *       printf("Session active, %u packets received\n", info.packets_received);
 *   }
 */
int icmp_get_session(icmp_server_t *server, uint16_t session_id, 
                      icmp_session_info_t *session_info);

/**
 * Get all active sessions
 * 
 * @param server ICMP server context
 * @param sessions Output: array of session info (must be at least ICMP_MAX_SESSIONS)
 * @param count Output: number of active sessions
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   icmp_session_info_t sessions[ICMP_MAX_SESSIONS];
 *   size_t count;
 *   icmp_get_all_sessions(server, sessions, &count);
 *   printf("Active sessions: %zu\n", count);
 */
int icmp_get_all_sessions(icmp_server_t *server, icmp_session_info_t *sessions, 
                           size_t *count);

/**
 * Remove inactive sessions (timeout cleanup)
 * 
 * @param server ICMP server context
 * 
 * @return Number of sessions removed
 * 
 * Note: Removes sessions that haven't been active for ICMP_SESSION_TIMEOUT_SEC
 */
int icmp_cleanup_sessions(icmp_server_t *server);

/**
 * Get server statistics
 * 
 * @param server ICMP server context
 * @param packets_received Output: total packets received
 * @param packets_sent Output: total packets sent
 * @param bytes_received Output: total bytes received
 * @param bytes_sent Output: total bytes sent
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 */
int icmp_get_statistics(icmp_server_t *server, uint32_t *packets_received,
                         uint32_t *packets_sent, uint64_t *bytes_received,
                         uint64_t *bytes_sent);

/**
 * Destroy ICMP server and free resources
 * 
 * @param server ICMP server context
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Closes socket and frees memory
 */
int icmp_server_destroy(icmp_server_t *server);

/**
 * Get error message string for error code
 * 
 * @param error_code Error code from ICMP functions
 * @return Human-readable error message
 */
const char* icmp_server_error_string(int error_code);

/**
 * Calculate ICMP checksum (RFC 1071)
 * 
 * @param data Pointer to data
 * @param len Length of data in bytes
 * 
 * @return 16-bit checksum
 * 
 * Note: Used internally, exposed for testing
 */
uint16_t icmp_checksum(const uint8_t *data, size_t len);

/**
 * Set verbose logging
 * 
 * @param verbose 1 to enable verbose logs, 0 to disable
 */
void icmp_set_verbose(int verbose);

#endif // ICMP_HANDLER_H
