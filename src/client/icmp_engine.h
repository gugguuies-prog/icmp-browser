/**
 * icmp_engine.h
 * 
 * ICMP Engine for Windows client (using Icmp.dll API)
 * 
 * Features:
 * - Wrapper around Windows Icmp.dll (IcmpSendEcho, IcmpSendEcho2)
 * - Multi-threaded (send + receive threads)
 * - Session management with sequence numbers
 * - Timeout and retransmission support
 * - No administrator privileges required
 */

#ifndef ICMP_ENGINE_H
#define ICMP_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#else
#error "This ICMP engine is for Windows only. Use icmp_handler.h for Linux."
#endif

#include <stdint.h>
#include <stddef.h>

// Constants
#define ICMP_MTU 1472                    // Maximum payload size (1500 - 20 IP - 8 ICMP)
#define ICMP_TIMEOUT_MS 5000             // Default timeout (5 seconds)
#define ICMP_MAX_RETRIES 3               // Maximum retransmission attempts
#define ICMP_REPLY_BUFFER_SIZE 2048      // Buffer for reply packets

// Error codes
#define ICMP_SUCCESS 0
#define ICMP_ERROR_INVALID_PARAM -1
#define ICMP_ERROR_CREATE_FAILED -2
#define ICMP_ERROR_SEND_FAILED -3
#define ICMP_ERROR_RECV_FAILED -4
#define ICMP_ERROR_TIMEOUT -5
#define ICMP_ERROR_MEMORY -6
#define ICMP_ERROR_THREAD -7
#define ICMP_ERROR_SESSION_NOT_FOUND -8

/**
 * ICMP packet structure
 */
typedef struct {
    uint8_t type;                        // ICMP type (8=Echo Request, 0=Echo Reply)
    uint16_t id;                         // Session ID
    uint16_t sequence;                   // Sequence number
    uint8_t *payload;                    // Payload data (encrypted IP packet)
    size_t payload_size;                 // Size of payload
} icmp_packet_t;

/**
 * ICMP session structure
 * Manages connection to a remote server via ICMP
 */
typedef struct icmp_session_s {
    // Windows ICMP handle
    HANDLE hIcmp;                        // IcmpCreateFile handle
    
    // Server information
    struct sockaddr_in server_addr;      // Server IP address
    char server_ip[16];                  // Server IP as string
    
    // Session parameters
    uint16_t session_id;                 // Unique session ID
    uint16_t sequence_counter;           // Packet sequence counter
    uint32_t timeout_ms;                 // Timeout in milliseconds
    uint8_t max_retries;                 // Maximum retry attempts
    
    // Threading
    CRITICAL_SECTION send_lock;          // Lock for sending
    CRITICAL_SECTION recv_lock;          // Lock for receiving
    HANDLE send_thread;                  // Send thread handle
    HANDLE recv_thread;                  // Receive thread handle
    BOOL running;                        // Thread running flag
    
    // Statistics
    uint32_t packets_sent;               // Total packets sent
    uint32_t packets_received;           // Total packets received
    uint32_t packets_lost;               // Packets lost (timeout)
    uint32_t total_rtt_ms;               // Total RTT (for average calculation)
    
    // Reply buffer
    uint8_t reply_buffer[ICMP_REPLY_BUFFER_SIZE];
    
} icmp_session_t;

/**
 * Create ICMP session to a server
 * 
 * @param server_ip Server IP address (e.g., "192.168.1.100")
 * 
 * @return Pointer to icmp_session_t on success, NULL on error
 * 
 * Note: Does NOT require administrator privileges
 * 
 * Example:
 *   icmp_session_t *session = icmp_session_create("192.168.1.100");
 *   if (session) {
 *       // Use session
 *       icmp_session_destroy(session);
 *   }
 */
icmp_session_t* icmp_session_create(const char *server_ip);

/**
 * Send ICMP Echo Request packet
 * 
 * @param session ICMP session
 * @param payload Payload data to send (already encrypted)
 * @param payload_size Size of payload (max ICMP_MTU)
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Automatically increments sequence number
 *       Thread-safe (uses send_lock)
 * 
 * Example:
 *   uint8_t data[100] = { 0x01, 0x02, 0x03 }; // encrypted IP packet
 *   int result = icmp_send_packet(session, data, 100);
 */
int icmp_send_packet(icmp_session_t *session, const uint8_t *payload, size_t payload_size);

/**
 * Wait for ICMP Echo Reply packet
 * 
 * @param session      ICMP session
 * @param reply_payload Output buffer for reply payload
 * @param reply_size   Output: size of reply payload
 * @param timeout_ms   Timeout in milliseconds (0 = use session default)
 * 
 * @return ICMP_SUCCESS on success, ICMP_ERROR_TIMEOUT if timeout, error code otherwise
 * 
 * Note: Blocks until reply received or timeout
 *       Thread-safe (uses recv_lock)
 * 
 * Example:
 *   uint8_t reply[ICMP_MTU];
 *   size_t reply_size;
 *   int result = icmp_wait_reply(session, reply, &reply_size, 5000);
 *   if (result == ICMP_SUCCESS) {
 *       // Process reply
 *   }
 */
int icmp_wait_reply(icmp_session_t *session, uint8_t *reply_payload, size_t *reply_size, uint32_t timeout_ms);

/**
 * Send packet with retransmission on timeout
 * 
 * @param session ICMP session
 * @param payload Payload data to send
 * @param payload_size Size of payload
 * @param reply_payload Output buffer for reply (can be NULL if no reply expected)
 * @param reply_size Output: size of reply (can be NULL)
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Automatically retries up to max_retries times with exponential backoff
 * 
 * Example:
 *   uint8_t data[100], reply[ICMP_MTU];
 *   size_t reply_size;
 *   int result = icmp_send_reliable(session, data, 100, reply, &reply_size);
 */
int icmp_send_reliable(icmp_session_t *session, const uint8_t *payload, size_t payload_size,
                        uint8_t *reply_payload, size_t *reply_size);

/**
 * Get session statistics
 * 
 * @param session ICMP session
 * @param packets_sent Output: total packets sent
 * @param packets_received Output: total packets received
 * @param packets_lost Output: packets lost
 * @param avg_rtt_ms Output: average RTT in milliseconds
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 */
int icmp_get_statistics(icmp_session_t *session, uint32_t *packets_sent, 
                         uint32_t *packets_received, uint32_t *packets_lost,
                         uint32_t *avg_rtt_ms);

/**
 * Test connectivity to server (simple ping)
 * 
 * @param session ICMP session
 * @param rtt_ms Output: round-trip time in milliseconds (can be NULL)
 * 
 * @return ICMP_SUCCESS if server responds, error code otherwise
 * 
 * Example:
 *   uint32_t rtt;
 *   if (icmp_test_connectivity(session, &rtt) == ICMP_SUCCESS) {
 *       printf("Server reachable, RTT: %u ms\n", rtt);
 *   }
 */
int icmp_test_connectivity(icmp_session_t *session, uint32_t *rtt_ms);

/**
 * Set session parameters
 * 
 * @param session ICMP session
 * @param timeout_ms Timeout in milliseconds (0 = no change)
 * @param max_retries Maximum retry attempts (0 = no change)
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 */
int icmp_set_parameters(icmp_session_t *session, uint32_t timeout_ms, uint8_t max_retries);

/**
 * Destroy ICMP session and free resources
 * 
 * @param session ICMP session to destroy
 * 
 * @return ICMP_SUCCESS on success, error code otherwise
 * 
 * Note: Closes ICMP handle and frees memory
 */
int icmp_session_destroy(icmp_session_t *session);

/**
 * Get error message string for error code
 * 
 * @param error_code Error code from ICMP functions
 * @return Human-readable error message
 */
const char* icmp_error_string(int error_code);

/**
 * Get last Windows error as string
 * 
 * @return String describing last Windows error
 * 
 * Note: Useful for debugging IcmpSendEcho failures
 */
const char* icmp_get_last_windows_error();

#ifdef __cplusplus
}
#endif

#endif // ICMP_ENGINE_H
