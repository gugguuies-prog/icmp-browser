/**
 * session_manager.c
 * 
 * Session Management Implementation for ICMP Tunnel Server
 */

#include "session_manager.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

/**
 * Initialize session manager
 */
int session_manager_init(session_manager_t *manager) {
    if (!manager) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    // Zero-initialize all sessions
    memset(manager->sessions, 0, sizeof(manager->sessions));
    
    // Initialize mutex
    if (pthread_mutex_init(&manager->lock, NULL) != 0) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    manager->active_count = 0;
    
    return SESSION_SUCCESS;
}

/**
 * Create new session
 */
int session_manager_create(session_manager_t *manager, uint16_t session_id,
                            const struct sockaddr_in *client_addr) {
    if (!manager || !client_addr) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Check if session already exists
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            pthread_mutex_unlock(&manager->lock);
            return SESSION_ERROR_ALREADY_EXISTS;
        }
    }
    
    // Find free slot
    int slot = -1;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (!manager->sessions[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_TABLE_FULL;
    }
    
    // Initialize session
    session_entry_t *session = &manager->sessions[slot];
    memset(session, 0, sizeof(session_entry_t));
    
    session->session_id = session_id;
    memcpy(&session->client_addr, client_addr, sizeof(struct sockaddr_in));
    session->state = SESSION_STATE_INIT;
    session->key_established = 0;
    session->created_time = time(NULL);
    session->last_activity = session->created_time;
    session->active = 1;
    
    manager->active_count++;
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Set session key (after handshake completion)
 */
int session_manager_set_key(session_manager_t *manager, uint16_t session_id,
                              const uint8_t session_key[32]) {
    if (!manager || !session_key) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Set key
    memcpy(session->session_key, session_key, 32);
    session->key_established = 1;
    session->state = SESSION_STATE_ACTIVE;
    session->last_activity = time(NULL);
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Get session key
 */
int session_manager_get_key(session_manager_t *manager, uint16_t session_id,
                              uint8_t session_key[32]) {
    if (!manager || !session_key) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    if (!session->key_established) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Get key
    memcpy(session_key, session->session_key, 32);
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Find session by client address
 */
int session_manager_find_by_addr(session_manager_t *manager,
                                   const struct sockaddr_in *client_addr,
                                   uint16_t *session_id) {
    if (!manager || !client_addr || !session_id) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session by IP address and port
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active) {
            session_entry_t *session = &manager->sessions[i];
            if (session->client_addr.sin_addr.s_addr == client_addr->sin_addr.s_addr &&
                session->client_addr.sin_port == client_addr->sin_port) {
                *session_id = session->session_id;
                pthread_mutex_unlock(&manager->lock);
                return SESSION_SUCCESS;
            }
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    return SESSION_ERROR_NOT_FOUND;
}

/**
 * Update session activity (refresh timeout)
 */
int session_manager_update_activity(session_manager_t *manager, uint16_t session_id) {
    if (!manager) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Update last activity
    session->last_activity = time(NULL);
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Update session statistics
 */
int session_manager_update_stats(session_manager_t *manager, uint16_t session_id,
                                   uint32_t packets_received, uint32_t packets_sent,
                                   uint64_t bytes_received, uint64_t bytes_sent) {
    if (!manager) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Update statistics
    session->packets_received += packets_received;
    session->packets_sent += packets_sent;
    session->bytes_received += bytes_received;
    session->bytes_sent += bytes_sent;
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Get session info
 */
int session_manager_get_info(session_manager_t *manager, uint16_t session_id,
                               session_entry_t *entry) {
    if (!manager || !entry) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Copy session info (but NOT the key for security)
    memcpy(entry, session, sizeof(session_entry_t));
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Get all active sessions
 */
int session_manager_get_all(session_manager_t *manager, session_entry_t *sessions,
                             size_t *count, size_t max_sessions) {
    if (!manager || !sessions || !count) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    size_t n = 0;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS && n < max_sessions; i++) {
        if (manager->sessions[i].active) {
            memcpy(&sessions[n], &manager->sessions[i], sizeof(session_entry_t));
            n++;
        }
    }
    
    *count = n;
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Remove session
 */
int session_manager_remove(session_manager_t *manager, uint16_t session_id) {
    if (!manager) {
        return SESSION_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Find session
    session_entry_t *session = NULL;
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active && 
            manager->sessions[i].session_id == session_id) {
            session = &manager->sessions[i];
            break;
        }
    }
    
    if (!session) {
        pthread_mutex_unlock(&manager->lock);
        return SESSION_ERROR_NOT_FOUND;
    }
    
    // Securely wipe session key
    memset(session->session_key, 0, sizeof(session->session_key));
    
    // Mark session as inactive
    session->active = 0;
    session->state = SESSION_STATE_CLOSED;
    
    manager->active_count--;
    
    pthread_mutex_unlock(&manager->lock);
    
    return SESSION_SUCCESS;
}

/**
 * Cleanup expired sessions (timeout)
 */
int session_manager_cleanup(session_manager_t *manager) {
    if (!manager) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    time_t now = time(NULL);
    int removed = 0;
    
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active) {
            session_entry_t *session = &manager->sessions[i];
            
            // Check timeout
            if ((now - session->last_activity) > SESSION_TIMEOUT_SEC) {
                // Securely wipe session key
                memset(session->session_key, 0, sizeof(session->session_key));
                
                // Mark as timed out
                session->active = 0;
                session->state = SESSION_STATE_TIMEOUT;
                
                manager->active_count--;
                removed++;
            }
        }
    }
    
    pthread_mutex_unlock(&manager->lock);
    
    return removed;
}

/**
 * Get active session count
 */
int session_manager_count(session_manager_t *manager) {
    if (!manager) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->lock);
    int count = manager->active_count;
    pthread_mutex_unlock(&manager->lock);
    
    return count;
}

/**
 * Destroy session manager
 */
void session_manager_destroy(session_manager_t *manager) {
    if (!manager) {
        return;
    }
    
    pthread_mutex_lock(&manager->lock);
    
    // Securely wipe all session keys
    for (size_t i = 0; i < SESSION_MAX_SESSIONS; i++) {
        if (manager->sessions[i].active) {
            memset(manager->sessions[i].session_key, 0, 
                   sizeof(manager->sessions[i].session_key));
            manager->sessions[i].active = 0;
        }
    }
    
    manager->active_count = 0;
    
    pthread_mutex_unlock(&manager->lock);
    
    // Destroy mutex
    pthread_mutex_destroy(&manager->lock);
}

/**
 * Get error message string
 */
const char* session_manager_error_string(int error_code) {
    switch (error_code) {
        case SESSION_SUCCESS:
            return "Success";
        case SESSION_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case SESSION_ERROR_TABLE_FULL:
            return "Session table full";
        case SESSION_ERROR_NOT_FOUND:
            return "Session not found";
        case SESSION_ERROR_ALREADY_EXISTS:
            return "Session already exists";
        case SESSION_ERROR_TIMEOUT:
            return "Session timeout";
        default:
            return "Unknown error";
    }
}

/**
 * Get state string
 */
const char* session_state_string(session_state_t state) {
    switch (state) {
        case SESSION_STATE_INIT:
            return "INIT";
        case SESSION_STATE_HANDSHAKE_SENT:
            return "HANDSHAKE_SENT";
        case SESSION_STATE_ACTIVE:
            return "ACTIVE";
        case SESSION_STATE_TIMEOUT:
            return "TIMEOUT";
        case SESSION_STATE_CLOSED:
            return "CLOSED";
        default:
            return "UNKNOWN";
    }
}
