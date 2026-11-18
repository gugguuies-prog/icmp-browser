/**
 * session_manager.h
 * 
 * Session Management for ICMP Tunnel Server
 * 
 * Features:
 * - Session table with negotiated keys
 * - Client address to session mapping
 * - Session timeout and cleanup
 * - Thread-safe operations
 * - Statistics per session
 */

#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <netinet/in.h>
#include "../common/thread_compat.h"

// Constants
#define SESSION_MAX_SESSIONS 256         // Maximum concurrent sessions
#define SESSION_TIMEOUT_SEC 300          // Session timeout (5 minutes)
#define SESSION_KEY_SIZE 32              // AES-256 key size

// Error codes
#define SESSION_SUCCESS 0
#define SESSION_ERROR_INVALID_PARAM -1
#define SESSION_ERROR_TABLE_FULL -2
#define SESSION_ERROR_NOT_FOUND -3
#define SESSION_ERROR_ALREADY_EXISTS -4
#define SESSION_ERROR_TIMEOUT -5

/**
 * Session state
 */
typedef enum {
    SESSION_STATE_INIT,                  // Initial state (handshake not done)
    SESSION_STATE_HANDSHAKE_SENT,        // Server sent handshake response
    SESSION_STATE_ACTIVE,                // Session active (key negotiated)
    SESSION_STATE_TIMEOUT,               // Session timed out
    SESSION_STATE_CLOSED                 // Session closed
} session_state_t;

/**
 * Session entry
 */
typedef struct {
    // Session identification
    uint16_t session_id;                 // Session ID
    struct sockaddr_in client_addr;      // Client IP address
    session_state_t state;               // Session state
    
    // Cryptographic keys
    uint8_t session_key[32];             // AES-256 session key
    uint8_t key_established;             // 1 if key is established
    
    // Timing
    time_t created_time;                 // Session creation timestamp
    time_t last_activity;                // Last activity timestamp
    
    // Statistics
    uint64_t packets_received;           // Total packets received
    uint64_t packets_sent;               // Total packets sent
    uint64_t bytes_received;             // Total bytes received
    uint64_t bytes_sent;                 // Total bytes sent
    
    // Flags
    uint8_t active;                      // 1 if session is active
    
} session_entry_t;

/**
 * Session manager context
 */
typedef struct {
    session_entry_t sessions[SESSION_MAX_SESSIONS];  // Session table
    mutex_t lock;                                    // Thread-safe lock
    uint32_t active_count;                           // Number of active sessions
} session_manager_t;

/**
 * Initialize session manager
 * 
 * @param manager Session manager context
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_init(session_manager_t *manager);

/**
 * Create new session
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * @param client_addr Client IP address
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   struct sockaddr_in client_addr;
 *   session_manager_create(&manager, 0x1234, &client_addr);
 */
int session_manager_create(session_manager_t *manager, uint16_t session_id,
                            const struct sockaddr_in *client_addr);

/**
 * Set session key (after handshake completion)
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * @param session_key Session key (32 bytes)
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   uint8_t key[32];  // From handshake
 *   session_manager_set_key(&manager, 0x1234, key);
 */
int session_manager_set_key(session_manager_t *manager, uint16_t session_id,
                              const uint8_t session_key[32]);

/**
 * Get session key
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * @param session_key Output: session key (32 bytes)
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   uint8_t key[32];
 *   if (session_manager_get_key(&manager, 0x1234, key) == SESSION_SUCCESS) {
 *       // Use key for encryption/decryption
 *   }
 */
int session_manager_get_key(session_manager_t *manager, uint16_t session_id,
                              uint8_t session_key[32]);

/**
 * Find session by client address
 * 
 * @param manager Session manager
 * @param client_addr Client IP address
 * @param session_id Output: session ID if found
 * 
 * @return SESSION_SUCCESS if found, SESSION_ERROR_NOT_FOUND otherwise
 * 
 * Example:
 *   uint16_t sid;
 *   if (session_manager_find_by_addr(&manager, &client_addr, &sid) == SESSION_SUCCESS) {
 *       // Session exists
 *   }
 */
int session_manager_find_by_addr(session_manager_t *manager,
                                   const struct sockaddr_in *client_addr,
                                   uint16_t *session_id);

/**
 * Update session activity (refresh timeout)
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_update_activity(session_manager_t *manager, uint16_t session_id);

/**
 * Update session statistics
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * @param packets_received Number of packets received (increment)
 * @param packets_sent Number of packets sent (increment)
 * @param bytes_received Bytes received (increment)
 * @param bytes_sent Bytes sent (increment)
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_update_stats(session_manager_t *manager, uint16_t session_id,
                                   uint32_t packets_received, uint32_t packets_sent,
                                   uint64_t bytes_received, uint64_t bytes_sent);

/**
 * Get session info
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * @param entry Output: session entry copy
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_get_info(session_manager_t *manager, uint16_t session_id,
                               session_entry_t *entry);

/**
 * Get all active sessions
 * 
 * @param manager Session manager
 * @param sessions Output: array of session entries
 * @param count Output: number of active sessions
 * @param max_sessions Maximum sessions to return
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_get_all(session_manager_t *manager, session_entry_t *sessions,
                             size_t *count, size_t max_sessions);

/**
 * Remove session
 * 
 * @param manager Session manager
 * @param session_id Session ID
 * 
 * @return SESSION_SUCCESS on success, error code otherwise
 */
int session_manager_remove(session_manager_t *manager, uint16_t session_id);

/**
 * Cleanup expired sessions (timeout)
 * 
 * @param manager Session manager
 * 
 * @return Number of sessions removed
 */
int session_manager_cleanup(session_manager_t *manager);

/**
 * Get active session count
 * 
 * @param manager Session manager
 * 
 * @return Number of active sessions
 */
int session_manager_count(session_manager_t *manager);

/**
 * Destroy session manager
 * 
 * @param manager Session manager
 * 
 * Note: Securely wipes all session keys
 */
void session_manager_destroy(session_manager_t *manager);

/**
 * Get error message string
 * 
 * @param error_code Error code
 * @return Human-readable error message
 */
const char* session_manager_error_string(int error_code);

/**
 * Get state string
 * 
 * @param state Session state
 * @return Human-readable state string
 */
const char* session_state_string(session_state_t state);

#endif // SESSION_MANAGER_H
