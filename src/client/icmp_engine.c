/**
 * icmp_engine.c
 * 
 * Implementation of ICMP Engine for Windows client
 */

#include "icmp_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper: Generate random session ID
static uint16_t generate_session_id() {
    srand((unsigned int)time(NULL));
    return (uint16_t)(rand() % 0xFFFF);
}

/**
 * Create ICMP session
 */
icmp_session_t* icmp_session_create(const char *server_ip) {
    if (!server_ip) {
        return NULL;
    }

    icmp_session_t *session = (icmp_session_t*)calloc(1, sizeof(icmp_session_t));
    if (!session) {
        return NULL;
    }

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        free(session);
        return NULL;
    }

    // Create ICMP handle (NO ADMIN REQUIRED!)
    session->hIcmp = IcmpCreateFile();
    if (session->hIcmp == INVALID_HANDLE_VALUE) {
        WSACleanup();
        free(session);
        return NULL;
    }

    // Parse server IP
    session->server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, server_ip, &session->server_addr.sin_addr) != 1) {
        IcmpCloseHandle(session->hIcmp);
        WSACleanup();
        free(session);
        return NULL;
    }
    strncpy(session->server_ip, server_ip, sizeof(session->server_ip) - 1);

    // Initialize session parameters
    // Note: Windows IcmpSendEcho always uses id=1, so we must use id=1 too
    session->session_id = 1;  // Fixed ID to match Windows ICMP behavior
    session->sequence_counter = 0;
    session->timeout_ms = ICMP_TIMEOUT_MS;
    session->max_retries = ICMP_MAX_RETRIES;
    session->running = TRUE;

    // Initialize locks
    InitializeCriticalSection(&session->send_lock);
    InitializeCriticalSection(&session->recv_lock);

    // Initialize statistics
    session->packets_sent = 0;
    session->packets_received = 0;
    session->packets_lost = 0;
    session->total_rtt_ms = 0;

    return session;
}

/**
 * Send ICMP Echo Request packet
 */
int icmp_send_packet(icmp_session_t *session, const uint8_t *payload, size_t payload_size) {
    if (!session || !session->hIcmp || session->hIcmp == INVALID_HANDLE_VALUE) {
        return ICMP_ERROR_INVALID_PARAM;
    }
    if (!payload || payload_size == 0 || payload_size > ICMP_MTU) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    EnterCriticalSection(&session->send_lock);

    // Prepare IP options
    IP_OPTION_INFORMATION ipOptions;
    memset(&ipOptions, 0, sizeof(ipOptions));
    ipOptions.Ttl = 128;
    ipOptions.Tos = 0;
    ipOptions.Flags = 0;

    // Send ICMP Echo Request
    DWORD dwRetVal = IcmpSendEcho(
        session->hIcmp,
        session->server_addr.sin_addr.S_un.S_addr,
        (LPVOID)payload,
        (WORD)payload_size,
        &ipOptions,
        session->reply_buffer,
        ICMP_REPLY_BUFFER_SIZE,
        session->timeout_ms
    );
    
    // Debug: log what we sent
    char debug_send[256];
    sprintf_s(debug_send, sizeof(debug_send),
             "ICMP Send: payload_size=%zu, first 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
             payload_size,
             payload[0], payload[1], payload[2], payload[3],
             payload[4], payload[5], payload[6], payload[7]);
    OutputDebugStringA(debug_send);

    int result = ICMP_SUCCESS;
    if (dwRetVal == 0) {
        DWORD error = GetLastError();
        if (error == IP_REQ_TIMED_OUT) {
            result = ICMP_ERROR_TIMEOUT;
            session->packets_lost++;
        } else {
            result = ICMP_ERROR_SEND_FAILED;
        }
    } else {
        session->packets_sent++;
        session->packets_received++;
        
        // Extract RTT
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)session->reply_buffer;
        session->total_rtt_ms += pEchoReply->RoundTripTime;
    }

    session->sequence_counter++;
    LeaveCriticalSection(&session->send_lock);

    return result;
}

/**
 * Wait for ICMP Echo Reply
 */
int icmp_wait_reply(icmp_session_t *session, uint8_t *reply_payload, 
                     size_t *reply_size, uint32_t timeout_ms) {
    if (!session || !reply_payload || !reply_size) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    EnterCriticalSection(&session->recv_lock);

    // Use session timeout if not specified
    if (timeout_ms == 0) {
        timeout_ms = session->timeout_ms;
    }

    // Parse the reply buffer from last send
    PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)session->reply_buffer;
    
    // Debug: log reply details
    char debug[512];
    sprintf_s(debug, sizeof(debug),
             "ICMP Reply: Status=%lu, RTT=%lu ms, DataSize=%d, Address=%08X",
             pEchoReply->Status, pEchoReply->RoundTripTime, 
             pEchoReply->DataSize, pEchoReply->Address);
    OutputDebugStringA(debug);
    
    if (pEchoReply->Status == IP_SUCCESS && pEchoReply->DataSize > 0) {
        // Copy payload
        *reply_size = pEchoReply->DataSize;
        if (*reply_size > ICMP_MTU) {
            *reply_size = ICMP_MTU;
        }
        memcpy(reply_payload, pEchoReply->Data, *reply_size);
        
        LeaveCriticalSection(&session->recv_lock);
        return ICMP_SUCCESS;
    }

    LeaveCriticalSection(&session->recv_lock);
    return ICMP_ERROR_RECV_FAILED;
}

/**
 * Send with retransmission (reliable)
 */
int icmp_send_reliable(icmp_session_t *session, const uint8_t *payload, size_t payload_size,
                        uint8_t *reply_payload, size_t *reply_size) {
    if (!session || !payload || payload_size == 0) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    int result;
    uint32_t delay_ms = 100; // Initial delay

    for (uint8_t attempt = 0; attempt < session->max_retries; attempt++) {
        // Send packet
        result = icmp_send_packet(session, payload, payload_size);
        
        if (result == ICMP_SUCCESS) {
            // If reply expected, extract it
            if (reply_payload && reply_size) {
                result = icmp_wait_reply(session, reply_payload, reply_size, 0);
                if (result == ICMP_SUCCESS) {
                    return ICMP_SUCCESS;
                }
            } else {
                return ICMP_SUCCESS;
            }
        }

        // Exponential backoff
        if (attempt < session->max_retries - 1) {
            Sleep(delay_ms);
            delay_ms = delay_ms * 2;
            if (delay_ms > 5000) delay_ms = 5000; // Cap at 5 seconds
        }
    }

    // All retries failed
    return result;
}

/**
 * Get session statistics
 */
int icmp_get_statistics(icmp_session_t *session, uint32_t *packets_sent, 
                         uint32_t *packets_received, uint32_t *packets_lost,
                         uint32_t *avg_rtt_ms) {
    if (!session) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    if (packets_sent) *packets_sent = session->packets_sent;
    if (packets_received) *packets_received = session->packets_received;
    if (packets_lost) *packets_lost = session->packets_lost;
    
    if (avg_rtt_ms) {
        if (session->packets_received > 0) {
            *avg_rtt_ms = session->total_rtt_ms / session->packets_received;
        } else {
            *avg_rtt_ms = 0;
        }
    }

    return ICMP_SUCCESS;
}

/**
 * Test connectivity (simple ping)
 */
int icmp_test_connectivity(icmp_session_t *session, uint32_t *rtt_ms) {
    if (!session) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    // Send a simple test payload
    const char *test_payload = "PING_TEST";
    uint8_t reply[ICMP_MTU];
    size_t reply_size;

    DWORD start_time = GetTickCount();
    int result = icmp_send_reliable(session, (const uint8_t*)test_payload, 
                                      strlen(test_payload), reply, &reply_size);
    DWORD end_time = GetTickCount();

    if (result == ICMP_SUCCESS && rtt_ms) {
        *rtt_ms = end_time - start_time;
    }

    return result;
}

/**
 * Set session parameters
 */
int icmp_set_parameters(icmp_session_t *session, uint32_t timeout_ms, uint8_t max_retries) {
    if (!session) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    if (timeout_ms > 0) {
        session->timeout_ms = timeout_ms;
    }
    if (max_retries > 0) {
        session->max_retries = max_retries;
    }

    return ICMP_SUCCESS;
}

/**
 * Destroy ICMP session
 */
int icmp_session_destroy(icmp_session_t *session) {
    if (!session) {
        return ICMP_ERROR_INVALID_PARAM;
    }

    session->running = FALSE;

    // Close ICMP handle
    if (session->hIcmp && session->hIcmp != INVALID_HANDLE_VALUE) {
        IcmpCloseHandle(session->hIcmp);
    }

    // Delete critical sections
    DeleteCriticalSection(&session->send_lock);
    DeleteCriticalSection(&session->recv_lock);

    // Cleanup Winsock
    WSACleanup();

    // Free memory
    memset(session, 0, sizeof(icmp_session_t));
    free(session);

    return ICMP_SUCCESS;
}

/**
 * Get error message string
 */
const char* icmp_error_string(int error_code) {
    switch (error_code) {
        case ICMP_SUCCESS:
            return "Success";
        case ICMP_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case ICMP_ERROR_CREATE_FAILED:
            return "Failed to create ICMP handle";
        case ICMP_ERROR_SEND_FAILED:
            return "Failed to send ICMP packet";
        case ICMP_ERROR_RECV_FAILED:
            return "Failed to receive ICMP reply";
        case ICMP_ERROR_TIMEOUT:
            return "Timeout waiting for reply";
        case ICMP_ERROR_MEMORY:
            return "Memory allocation failed";
        case ICMP_ERROR_THREAD:
            return "Thread operation failed";
        case ICMP_ERROR_SESSION_NOT_FOUND:
            return "Session not found";
        default:
            return "Unknown error";
    }
}

/**
 * Get last Windows error as string
 */
const char* icmp_get_last_windows_error() {
    static char error_msg[256];
    DWORD error = GetLastError();
    
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_msg,
        sizeof(error_msg),
        NULL
    );
    
    return error_msg;
}
