/**
 * main.c
 * 
 * ICMP Tunnel Server - Main Entry Point
 * Listens for ICMP Echo packets and tunnels IP traffic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "icmp_handler.h"
#include "session_manager.h"

// Global server handle for signal handler
static icmp_server_t *g_server = NULL;

/**
 * Signal handler for graceful shutdown
 */
void signal_handler(int signum) {
    printf("\n[Server] Received signal %d, shutting down...\n", signum);
    if (g_server) {
        icmp_server_stop(g_server);
    }
}

/**
 * Print usage
 */
void print_usage(const char *program_name) {
    printf("ICMP Tunnel Server\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -i <interface>  Network interface (default: eth0)\n");
    printf("  -p <port>       Control port (default: 8080)\n");
    printf("  -v              Verbose logging\n");
    printf("  -h              Show this help\n");
    printf("\nExample:\n");
    printf("  sudo %s -i eth0 -v\n", program_name);
}

int main(int argc, char *argv[]) {
    const char *interface = "eth0";
    int verbose = 0;
    int opt;
    
    printf("========================================\n");
    printf("  ICMP Tunnel Server\n");
    printf("========================================\n\n");
    
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "i:p:vh")) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "[Error] This program must be run as root (for raw ICMP sockets)\n");
        fprintf(stderr, "Try: sudo %s\n", argv[0]);
        return 1;
    }
    
    printf("[Server] Interface: %s\n", interface);
    printf("[Server] Verbose: %s\n", verbose ? "enabled" : "disabled");
    printf("\n");
    
    // Set verbose logging
    icmp_set_verbose(verbose);
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create ICMP server
    printf("[Server] Creating ICMP server...\n");
    g_server = icmp_server_create();
    if (!g_server) {
        fprintf(stderr, "[Error] Failed to create ICMP server\n");
        return 1;
    }
    
    printf("[Server] Note: Interface parameter '%s' will be used in future versions\n", interface);
    
    // Start server
    printf("[Server] Starting ICMP server...\n");
    if (icmp_server_start(g_server) != 0) {
        fprintf(stderr, "[Error] Failed to start ICMP server\n");
        icmp_server_destroy(g_server);
        return 1;
    }
    
    printf("[Server] ✓ Server running on %s\n", interface);
    printf("[Server] Press Ctrl+C to stop\n\n");
    
    // Main loop - wait for signals
    while (1) {
        sleep(1);
        
        // Print stats every 10 seconds
        static int counter = 0;
        if (++counter >= 10) {
            counter = 0;
            if (verbose) {
                // TODO: Add icmp_server_print_stats() function
                printf("[Server] Still running... (use -v for stats)\n");
            }
        }
    }
    
    // Cleanup (reached via signal handler)
    printf("[Server] Cleaning up...\n");
    icmp_server_destroy(g_server);
    printf("[Server] Stopped\n");
    
    return 0;
}
