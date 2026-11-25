/**
 * icmp_http_proxy_main.c
 * 
 * Standalone HTTP Proxy for ICMP Browser
 * 
 * Usage: icmp_http_proxy.exe [server_ip] [proxy_port]
 *        Default: server_ip=192.168.1.40, proxy_port=8888
 */

#include "http_proxy.h"
#include "icmp_engine.h"
#include "../common/handshake.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// Global state
static http_proxy_t *g_proxy = NULL;
static icmp_session_t *g_icmp_session = NULL;
static volatile int g_running = 1;
uint8_t g_session_key[32];  // Global session key (set after handshake)

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    (void)sig;
    printf("\n[Main] Shutdown signal received\n");
    g_running = 0;
}

/**
 * Helper: Perform complete DH handshake with server
 */
int client_perform_handshake(icmp_session_t *session, uint8_t session_key[32]) {
    handshake_context_t ctx;
    
    // Initialize handshake
    if (handshake_init(&ctx, session->session_id) != HANDSHAKE_SUCCESS) {
        return -1;
    }
    
    // Start handshake (generate client public key)
    proto_packet_t request_packet;
    if (handshake_client_start(&ctx, &request_packet) != HANDSHAKE_SUCCESS) {
        handshake_cleanup(&ctx);
        return -2;
    }
    
    printf("[Handshake] Sending client public key...");
    
    // Send handshake request via ICMP
    uint8_t send_buffer[2048];
    memcpy(send_buffer, &request_packet.header, sizeof(proto_header_t));
    memcpy(send_buffer + sizeof(proto_header_t), request_packet.payload, request_packet.payload_size);
    size_t send_size = sizeof(proto_header_t) + request_packet.payload_size;
    
    uint8_t reply_buffer[2048];
    size_t reply_size = 0;
    
    int result = icmp_send_reliable(session, send_buffer, send_size, reply_buffer, &reply_size);
    if (result != ICMP_SUCCESS || reply_size < sizeof(proto_header_t)) {
        printf("[Handshake] Failed to receive server response");
        handshake_cleanup(&ctx);
        return -3;
    }
    
    // Parse server response
    proto_packet_t response_packet;
    memcpy(&response_packet.header, reply_buffer, sizeof(proto_header_t));
    response_packet.payload_size = reply_size - sizeof(proto_header_t);
    memcpy(response_packet.payload, reply_buffer + sizeof(proto_header_t), response_packet.payload_size);
    
    printf("[Handshake] Received server public key, computing shared secret...");
    
    // Finish handshake (compute shared secret and derive key)
    if (handshake_client_finish(&ctx, &response_packet) != HANDSHAKE_SUCCESS) {
        handshake_cleanup(&ctx);
        return -4;
    }
    
    // Get session key
    handshake_get_session_key(&ctx, session_key);
    
    handshake_cleanup(&ctx);
    return 0;
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    const char *server_ip = "192.168.1.40";
    uint16_t proxy_port = 8888;
    
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        proxy_port = (uint16_t)atoi(argv[2]);
    }
    
    printf("+======================================================+\n");
    printf("|      ICMP HTTP Proxy - Standalone Version           |\n");
    printf("+======================================================+\n");
    printf("| Server:      %s                            \n", server_ip);
    printf("| Proxy Port:  %d                                      \n", proxy_port);
    printf("+======================================================+\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create ICMP session
    printf("[Main] Creating ICMP session to %s...\n", server_ip);
    g_icmp_session = icmp_session_create(server_ip);
    if (!g_icmp_session) {
        fprintf(stderr, "[Main] ERROR: Failed to create ICMP session\n");
        return 1;
    }
    
    printf("[Main] ICMP session created (ID: 0x%04x)\n", g_icmp_session->session_id);
    
    // Perform Diffie-Hellman handshake
    printf("[Main] Performing DH handshake...\n");
    
    uint8_t session_key[32];
    int handshake_result = client_perform_handshake(g_icmp_session, session_key);
    
    if (handshake_result != 0) {
        fprintf(stderr, "[Main] ERROR: Handshake failed (code %d)\n", handshake_result);
        icmp_session_destroy(g_icmp_session);
        return 1;
    }
    
    printf("[Main] √ Handshake complete, session key established\n\n");

    // Copy session key to global for use by proxy
    memcpy(g_session_key, session_key, 32);
    
    
    // Create HTTP proxy
    printf("[Main] Starting HTTP proxy server on 127.0.0.1:%d...\n", proxy_port);
    g_proxy = http_proxy_create(proxy_port, g_icmp_session);
    if (!g_proxy) {
        fprintf(stderr, "[Main] ERROR: Failed to create HTTP proxy\n");
        icmp_session_destroy(g_icmp_session);
        return 1;
    }
    
    // Start the proxy server
    int proxy_result = http_proxy_start(g_proxy);
    if (proxy_result != HTTP_PROXY_SUCCESS) {
        fprintf(stderr, "[Main] ERROR: Failed to start proxy (code %d)\n", proxy_result);
        http_proxy_destroy(g_proxy);
        icmp_session_destroy(g_icmp_session);
        return 1;
    }
    
    printf("[Main] √ HTTP proxy listening\n\n");
    printf("+======================================================+\n");
    printf("|  Proxy is running!                                   |\n");
    printf("+======================================================+\n");
    printf("|  Configure your browser to use:                      |\n");
    printf("|  HTTP Proxy: 127.0.0.1:%d                            \n", proxy_port);
    printf("|                                                        |\n");
    printf("|  Chrome:  Settings -> System -> Proxy settings         |\n");
    printf("|  Firefox: Settings -> Network -> Manual proxy          |\n");
    printf("|                                                        |\n");
    printf("|  Press Ctrl+C to stop                                |\n");
    printf("+======================================================+\n\n");
    
    // Main loop - print stats every 10 seconds
    while (g_running) {
        Sleep(10000);  // Sleep 10 seconds
        
        if (g_running) {
            http_proxy_print_stats(g_proxy);
        }
    }
    
    // Cleanup
    printf("\n[Main] Shutting down...\n");
    http_proxy_stop(g_proxy);
    http_proxy_destroy(g_proxy);
    icmp_session_destroy(g_icmp_session);
    
    printf("[Main] Goodbye!\n");
    return 0;
}
