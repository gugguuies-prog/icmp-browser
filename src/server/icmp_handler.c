/**
 * icmp_handler.c
 * 
 * Implementation of ICMP Handler for Linux server
 */

#include "icmp_handler.h"
#include "http_proxy.h"
#include "../common/protocol.h"
#include "../common/handshake.h"
#include "../crypto/aes_gcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/select.h>

// Debug logging
#define LOG_VERBOSE(...) if (g_verbose) printf(__VA_ARGS__)
static int g_verbose = 0;

void icmp_set_verbose(int verbose) {
    g_verbose = verbose;
}

/**
 * Calculate ICMP checksum (RFC 1071)
 */
uint16_t icmp_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t*)data;
    
    // Sum 16-bit words
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    // Add odd byte if present
    if (len > 0) {
        sum += *(uint8_t*)ptr;
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)~sum;
}

/**
 * Find or create session for client
 */
static icmp_session_info_t* find_or_create_session(icmp_server_t *server,
                                                     const struct sockaddr_in *client_addr,
                                                     uint16_t session_id) {
    mutex_lock(&server->sessions_lock);
    
    time_t now = time(NULL);
    icmp_session_info_t *session = NULL;
    icmp_session_info_t *free_slot = NULL;
    
    // Search for existing session or free slot
    for (int i = 0; i < ICMP_MAX_SESSIONS; i++) {
        if (server->sessions[i].active) {
            // Check if session matches
            if (server->sessions[i].session_id == session_id &&
                server->sessions[i].client_addr.sin_addr.s_addr == client_addr->sin_addr.s_addr) {
                session = &server->sessions[i];
                session->last_seen = now;
                break;
            }
        } else if (free_slot == NULL) {
            free_slot = &server->sessions[i];
        }
    }
    
    // Create new session if not found
    if (!session && free_slot) {
        session = free_slot;
        memset(session, 0, sizeof(icmp_session_info_t));
        session->client_addr = *client_addr;
        session->session_id = session_id;
        session->last_seen = now;
        session->active = 1;
        session->handshake_complete = 0;
        session->data_sequence_counter = 0;  // Start at 0 for DATA packets
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
        LOG_VERBOSE("[Server] New session created: ID=0x%04x from %s\n", session_id, client_ip);
    }
    
    mutex_unlock(&server->sessions_lock);
    
    return session;
}

/**
 * Handle handshake packet (Diffie-Hellman key exchange)
 */
static int handle_handshake(icmp_server_t *server, icmp_session_info_t *session,
                             const proto_packet_t *request, uint16_t icmp_sequence,
                             const struct sockaddr_in *client_addr) {
    LOG_VERBOSE("[Server] Processing handshake request for session 0x%04x\n", session->session_id);
    
    // Reset data sequence counter on new handshake (session restart)
    session->data_sequence_counter = 0;
    
    // Initialize handshake context for this session
    handshake_context_t hs_ctx;
    if (handshake_init(&hs_ctx, session->session_id) != HANDSHAKE_SUCCESS) {
        LOG_VERBOSE("[Server] Failed to initialize handshake context\n");
        return -1;
    }
    
    // Process handshake request and generate response
    proto_packet_t response;
    int result = handshake_server_process(&hs_ctx, request, &response);
    if (result != HANDSHAKE_SUCCESS) {
        LOG_VERBOSE("[Server] Handshake processing failed: %s\n", handshake_error_string(result));
        handshake_cleanup(&hs_ctx);
        return -1;
    }
    
    // Get the derived session key
    if (handshake_get_session_key(&hs_ctx, session->session_key) != HANDSHAKE_SUCCESS) {
        LOG_VERBOSE("[Server] Failed to get session key\n");
        handshake_cleanup(&hs_ctx);
        return -1;
    }
    
    session->handshake_complete = 1;
    LOG_VERBOSE("[Server] Handshake complete! Session key derived for 0x%04x\n", session->session_id);
    LOG_VERBOSE("[Server] Session key (first 8 bytes): %02X %02X %02X %02X %02X %02X %02X %02X\n",
                session->session_key[0], session->session_key[1], session->session_key[2], session->session_key[3],
                session->session_key[4], session->session_key[5], session->session_key[6], session->session_key[7]);
    
    // Send handshake response with correct sequence number
    uint8_t reply_payload[ICMP_MTU];
    memcpy(reply_payload, &response.header, 8);
    memcpy(reply_payload + 8, response.payload, response.payload_size);
    size_t total_size = 8 + response.payload_size;
    
    icmp_send_reply(server, client_addr, session->session_id, icmp_sequence, reply_payload, total_size);
    
    handshake_cleanup(&hs_ctx);
    return 0;
}

/**
 * Handle data packet (encrypted IP packet)
 */
static int handle_data_packet(icmp_server_t *server, icmp_session_info_t *session,
                               const proto_packet_t *packet, uint16_t icmp_sequence,
                               const struct sockaddr_in *client_addr) {
    if (!session->handshake_complete) {
        LOG_VERBOSE("[Server] Received DATA before handshake complete\n");
        return -1;
    }
    
    // Use session's internal counter for decryption, not ICMP sequence
    uint16_t data_seq = session->data_sequence_counter;
    
    LOG_VERBOSE("[Server] Processing DATA packet for session 0x%04x (ICMP seq=%d, data_seq=%d, frag_num=%d)\n", 
                session->session_id, icmp_sequence, data_seq, packet->header.fragment_num);
    
    // Decapsulate (decrypt) the packet
    // IMPORTANT: Use the session's data sequence counter for AAD
    uint8_t decrypted[2048];
    size_t decrypted_size;
    
    int result = proto_decapsulate(packet, session->session_key,
                                    session->session_id,
                                    data_seq,  // Use session counter, not ICMP sequence
                                    decrypted, &decrypted_size, sizeof(decrypted));
    
    if (result != PROTO_SUCCESS) {
        LOG_VERBOSE("[Server] Decapsulation failed: %s\n", proto_error_string(result));
        LOG_VERBOSE("[Server] Debug: session_id=0x%04x, data_seq=%d\n", session->session_id, data_seq);
        return -1;
    }
    
    LOG_VERBOSE("[Server] Decrypted %zu bytes\n", decrypted_size);
    
    // DON'T increment here - use same data_seq for response
    
    LOG_VERBOSE("[Server] Encrypting response with data_seq=%d\n", data_seq);
    
    // Process HTTP request via proxy
    uint8_t http_response_packet[4096];
    size_t http_response_size;
    
    if (http_proxy_process(decrypted, decrypted_size, 
                          http_response_packet, &http_response_size, 
                          sizeof(http_response_packet)) < 0) {
        LOG_VERBOSE("[Server] HTTP proxy failed, echoing request\n");
        // Fallback: echo the request back
        memcpy(http_response_packet, decrypted, decrypted_size);
        http_response_size = decrypted_size;
    }
    
    // Encapsulate the response
    proto_packet_t response_packets[10];
    size_t response_count;
    
    result = proto_encapsulate(http_response_packet, http_response_size,
                               session->session_key,
                               session->session_id,
                               data_seq,  // Use same data_seq as request
                               response_packets, &response_count, 10);
    
    if (result == PROTO_SUCCESS && response_count > 0) {
        // Send first fragment (simplified - should send all fragments)
        proto_packet_t *resp = &response_packets[0];
        uint8_t reply_payload[ICMP_MTU];
        memcpy(reply_payload, &resp->header, 8);
        memcpy(reply_payload + 8, resp->payload, resp->payload_size);
        size_t total_size = 8 + resp->payload_size;
        
        icmp_send_reply(server, client_addr, session->session_id, 
                       icmp_sequence, reply_payload, total_size);
        
        LOG_VERBOSE("[Server] Sent encrypted response (%zu bytes)\n", total_size);
        
        // NOW increment counter for next request/response pair
        session->data_sequence_counter++;
    }
    
    return 0;
}

/**
 * Create ICMP server
 */
icmp_server_t* icmp_server_create(void) {
    icmp_server_t *server = (icmp_server_t*)calloc(1, sizeof(icmp_server_t));
    if (!server) {
        return NULL;
    }
    
    // Create raw socket (requires root or CAP_NET_RAW)
    server->raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (server->raw_socket < 0) {
        free(server);
        return NULL;
    }
    
    // Initialize mutex
    if (mutex_init(&server->sessions_lock) != 0) {
        close(server->raw_socket);
        free(server);
        return NULL;
    }
    
    server->running = 0;
    
    return server;
}

/**
 * Send ICMP Echo Reply
 */
int icmp_send_reply(icmp_server_t *server, const struct sockaddr_in *client_addr,
                     uint16_t session_id, uint16_t sequence,
                     const uint8_t *payload, size_t payload_size) {
    if (!server || !client_addr || !payload || payload_size == 0 || payload_size > ICMP_MTU) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    // Build ICMP Echo Reply packet
    uint8_t packet[sizeof(icmp_header_t) + ICMP_MTU];
    icmp_header_t *icmp_hdr = (icmp_header_t*)packet;
    
    icmp_hdr->type = ICMP_TYPE_ECHO_REPLY;
    icmp_hdr->code = 0;
    icmp_hdr->checksum = 0;
    icmp_hdr->id = htons(session_id);
    icmp_hdr->sequence = htons(sequence);
    
    // Copy payload
    memcpy(packet + sizeof(icmp_header_t), payload, payload_size);
    
    // Calculate checksum
    size_t packet_size = sizeof(icmp_header_t) + payload_size;
    icmp_hdr->checksum = icmp_checksum(packet, packet_size);
    
    // Send packet
    ssize_t sent = sendto(server->raw_socket, packet, packet_size, 0,
                          (struct sockaddr*)client_addr, sizeof(*client_addr));
    
    if (sent < 0) {
        return ICMP_ERROR_SEND_FAILED;
    }
    
    // Update statistics
    server->total_packets_sent++;
    server->total_bytes_sent += sent;
    
    // Update session statistics
    icmp_session_info_t *session = find_or_create_session(server, client_addr, session_id);
    if (session) {
        session->packets_sent++;
        session->bytes_sent += sent;
    }
    
    return ICMP_SUCCESS;
}

/**
 * Process incoming ICMP packet
 */
static void process_icmp_packet(icmp_server_t *server, const uint8_t *buffer, 
                                 size_t size, const struct sockaddr_in *from_addr) {
    // Skip IP header (first 20 bytes typically)
    if (size < 20 + sizeof(icmp_header_t)) {
        return; // Packet too small
    }
    
    // Extract IP header length
    uint8_t ip_header_len = (buffer[0] & 0x0F) * 4;
    if (size < ip_header_len + sizeof(icmp_header_t)) {
        return;
    }
    
    // Parse ICMP header
    const icmp_header_t *icmp_hdr = (const icmp_header_t*)(buffer + ip_header_len);
    
    // Only process Echo Requests
    if (icmp_hdr->type != ICMP_TYPE_ECHO_REQUEST) {
        return;
    }
    
    // Extract payload
    size_t payload_offset = ip_header_len + sizeof(icmp_header_t);
    size_t payload_size = size - payload_offset;
    const uint8_t *payload = buffer + payload_offset;
    
    uint16_t session_id = ntohs(icmp_hdr->id);
    uint16_t sequence = ntohs(icmp_hdr->sequence);
    
    // Update session
    icmp_session_info_t *session = find_or_create_session(server, from_addr, session_id);
    if (!session) {
        LOG_VERBOSE("[Server] Failed to create session\n");
        return;
    }
    
    session->packets_received++;
    session->bytes_received += size;
    session->last_sequence = sequence;
    
    // Update global statistics
    server->total_packets_received++;
    server->total_bytes_received += size;
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from_addr->sin_addr, client_ip, sizeof(client_ip));
    LOG_VERBOSE("[Server] Received ICMP packet from %s (session=0x%04x, seq=%d, size=%zu)\n",
                client_ip, session_id, sequence, payload_size);
    
    // Parse protocol packet
    if (payload_size < 8) {
        LOG_VERBOSE("[Server] Payload too small\n");
        return;
    }
    
    proto_packet_t packet;
    memcpy(&packet.header, payload, 8);
    packet.payload_size = payload_size - 8;
    
    if (packet.payload_size > 0 && packet.payload_size <= PROTO_MAX_PAYLOAD) {
        memcpy(packet.payload, payload + 8, packet.payload_size);
    } else {
        LOG_VERBOSE("[Server] Invalid payload size: %zu\n", packet.payload_size);
        return;
    }
    
    LOG_VERBOSE("[Server] Protocol packet: type=%s, flags=0x%02x\n",
                proto_type_string(packet.header.type), packet.header.flags);
    
    // Handle packet based on type
    switch (packet.header.type) {
        case PROTO_TYPE_HANDSHAKE:
            handle_handshake(server, session, &packet, sequence, from_addr);
            break;
            
        case PROTO_TYPE_DATA:
            handle_data_packet(server, session, &packet, sequence, from_addr);
            break;
            
        case PROTO_TYPE_KEEPALIVE:
            LOG_VERBOSE("[Server] Keepalive received\n");
            // Just acknowledge
            icmp_send_reply(server, from_addr, session_id, sequence, payload, payload_size);
            break;
            
        default:
            LOG_VERBOSE("[Server] Unknown packet type: %d\n", packet.header.type);
            break;
    }
    
    // Call callback if registered
    if (server->on_packet_received) {
        server->on_packet_received(from_addr, payload, payload_size, session_id, sequence);
    }
}

/**
 * Start ICMP server (blocking)
 */
int icmp_server_start(icmp_server_t *server) {
    if (!server || server->raw_socket < 0) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    server->running = 1;
    
    struct sockaddr_in from_addr;
    socklen_t addr_len;
    fd_set read_fds;
    struct timeval timeout;
    
    while (server->running) {
        // Setup select() for non-blocking check
        FD_ZERO(&read_fds);
        FD_SET(server->raw_socket, &read_fds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(server->raw_socket + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, retry
            }
            return ICMP_ERROR_RECV_FAILED;
        }
        
        if (ready == 0) {
            // Timeout, cleanup old sessions
            icmp_cleanup_sessions(server);
            continue;
        }
        
        // Receive packet
        addr_len = sizeof(from_addr);
        ssize_t received = recvfrom(server->raw_socket, server->recv_buffer,
                                     ICMP_RECV_BUFFER_SIZE, 0,
                                     (struct sockaddr*)&from_addr, &addr_len);
        
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return ICMP_ERROR_RECV_FAILED;
        }
        
        if (received > 0) {
            process_icmp_packet(server, server->recv_buffer, received, &from_addr);
        }
    }
    
    return ICMP_SUCCESS;
}

/**
 * Stop ICMP server
 */
int icmp_server_stop(icmp_server_t *server) {
    if (!server) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    server->running = 0;
    return ICMP_SUCCESS;
}

/**
 * Set packet received callback
 */
int icmp_set_packet_callback(icmp_server_t *server,
                               void (*callback)(const struct sockaddr_in*, 
                                                const uint8_t*, size_t,
                                                uint16_t, uint16_t)) {
    if (!server) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    server->on_packet_received = callback;
    return ICMP_SUCCESS;
}

/**
 * Get session information
 */
int icmp_get_session(icmp_server_t *server, uint16_t session_id, 
                      icmp_session_info_t *session_info) {
    if (!server || !session_info) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&server->sessions_lock);
    
    for (int i = 0; i < ICMP_MAX_SESSIONS; i++) {
        if (server->sessions[i].active && server->sessions[i].session_id == session_id) {
            *session_info = server->sessions[i];
            mutex_unlock(&server->sessions_lock);
            return ICMP_SUCCESS;
        }
    }
    
    mutex_unlock(&server->sessions_lock);
    return ICMP_ERROR_SESSION_NOT_FOUND;
}

/**
 * Get all active sessions
 */
int icmp_get_all_sessions(icmp_server_t *server, icmp_session_info_t *sessions, 
                           size_t *count) {
    if (!server || !sessions || !count) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&server->sessions_lock);
    
    *count = 0;
    for (int i = 0; i < ICMP_MAX_SESSIONS; i++) {
        if (server->sessions[i].active) {
            sessions[*count] = server->sessions[i];
            (*count)++;
        }
    }
    
    mutex_unlock(&server->sessions_lock);
    return ICMP_SUCCESS;
}

/**
 * Remove inactive sessions
 */
int icmp_cleanup_sessions(icmp_server_t *server) {
    if (!server) {
        return 0;
    }
    
    mutex_lock(&server->sessions_lock);
    
    time_t now = time(NULL);
    int removed = 0;
    
    for (int i = 0; i < ICMP_MAX_SESSIONS; i++) {
        if (server->sessions[i].active) {
            if ((now - server->sessions[i].last_seen) > ICMP_SESSION_TIMEOUT_SEC) {
                server->sessions[i].active = 0;
                removed++;
            }
        }
    }
    
    mutex_unlock(&server->sessions_lock);
    
    return removed;
}

/**
 * Get server statistics
 */
int icmp_get_statistics(icmp_server_t *server, uint32_t *packets_received,
                         uint32_t *packets_sent, uint64_t *bytes_received,
                         uint64_t *bytes_sent) {
    if (!server) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    if (packets_received) *packets_received = server->total_packets_received;
    if (packets_sent) *packets_sent = server->total_packets_sent;
    if (bytes_received) *bytes_received = server->total_bytes_received;
    if (bytes_sent) *bytes_sent = server->total_bytes_sent;
    
    return ICMP_SUCCESS;
}

/**
 * Destroy ICMP server
 */
int icmp_server_destroy(icmp_server_t *server) {
    if (!server) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    
    server->running = 0;
    
    // Close socket
    if (server->raw_socket >= 0) {
        close(server->raw_socket);
    }
    
    // Destroy mutex
    mutex_destroy(&server->sessions_lock);
    
    // Free memory
    memset(server, 0, sizeof(icmp_server_t));
    free(server);
    
    return ICMP_SUCCESS;
}

/**
 * Get error message string
 */
const char* icmp_server_error_string(int error_code) {
    switch (error_code) {
        case ICMP_SUCCESS:
            return "Success";
        case ICMP_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case ICMP_ERROR_SOCKET_CREATE:
            return "Failed to create socket";
        case ICMP_ERROR_SOCKET_BIND:
            return "Failed to bind socket";
        case ICMP_ERROR_SEND_FAILED:
            return "Failed to send packet";
        case ICMP_ERROR_RECV_FAILED:
            return "Failed to receive packet";
        case ICMP_ERROR_TIMEOUT:
            return "Timeout";
        case ICMP_ERROR_MEMORY:
            return "Memory allocation failed";
        case ICMP_ERROR_SESSION_FULL:
            return "Session table full";
        case ICMP_ERROR_SESSION_NOT_FOUND:
            return "Session not found";
        case ICMP_ERROR_PERMISSION:
            return "Permission denied (need root or CAP_NET_RAW)";
        default:
            return "Unknown error";
    }
}
