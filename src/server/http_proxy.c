/**
 * http_proxy.c
 * 
 * HTTP proxy for ICMP tunnel server
 * Extracts HTTP requests from IP packets and performs real HTTP requests
 */

#include "http_proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_HTTP_RESPONSE 65536

/**
 * Extract HTTP request from IP/TCP packet
 */
static int extract_http_request(const uint8_t *ip_packet, size_t packet_size,
                                char *host, size_t host_size,
                                char *http_request, size_t request_size) {
    if (packet_size < 40) return -1;  // Minimum IP+TCP header
    
    struct iphdr *ip = (struct iphdr *)ip_packet;
    size_t ip_hdr_len = (ip->ihl) * 4;
    
    if (ip_hdr_len < 20 || ip_hdr_len > 60) return -1;
    if (packet_size < ip_hdr_len + 20) return -1;
    
    struct tcphdr *tcp = (struct tcphdr *)(ip_packet + ip_hdr_len);
    size_t tcp_hdr_len = (tcp->doff) * 4;
    
    if (tcp_hdr_len < 20) return -1;
    
    size_t http_start = ip_hdr_len + tcp_hdr_len;
    if (http_start >= packet_size) return -1;
    
    // Extract HTTP request
    size_t http_len = packet_size - http_start;
    if (http_len > request_size - 1) http_len = request_size - 1;
    
    memcpy(http_request, ip_packet + http_start, http_len);
    http_request[http_len] = '\0';
    
    // Extract Host header
    char *host_line = strstr(http_request, "Host: ");
    if (!host_line) host_line = strstr(http_request, "host: ");
    
    if (host_line) {
        host_line += 6;  // Skip "Host: "
        char *end = strstr(host_line, "\r\n");
        if (!end) end = strstr(host_line, "\n");
        
        if (end) {
            size_t len = end - host_line;
            if (len > host_size - 1) len = host_size - 1;
            memcpy(host, host_line, len);
            host[len] = '\0';
            return 0;
        }
    }
    
    return -1;
}

/**
 * Perform HTTP request and get response
 */
static int perform_http_request(const char *host, const char *http_request,
                                uint8_t *response, size_t *response_size, size_t max_size) {
    // Resolve host
    struct hostent *he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "[HTTP Proxy] Failed to resolve host: %s\n", host);
        return -1;
    }
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[HTTP Proxy] socket");
        return -1;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[HTTP Proxy] connect");
        close(sock);
        return -1;
    }
    
    // Send request
    size_t req_len = strlen(http_request);
    if (send(sock, http_request, req_len, 0) < 0) {
        perror("[HTTP Proxy] send");
        close(sock);
        return -1;
    }
    
    // Receive response
    size_t total_received = 0;
    ssize_t n;
    
    while (total_received < max_size - 1) {
        n = recv(sock, response + total_received, max_size - total_received - 1, 0);
        if (n <= 0) break;
        total_received += n;
    }
    
    close(sock);
    
    if (total_received == 0) {
        fprintf(stderr, "[HTTP Proxy] No response received\n");
        return -1;
    }
    
    response[total_received] = '\0';
    *response_size = total_received;
    
    printf("[HTTP Proxy] Received %zu bytes from %s\n", total_received, host);
    
    return 0;
}

/**
 * Create IP/TCP packet with HTTP response
 */
static size_t create_response_packet(const uint8_t *original_packet,
                                     const uint8_t *http_response, size_t http_size,
                                     uint8_t *output, size_t max_output) {
    if (http_size > 1400) http_size = 1400;  // Limit to MTU
    
    struct iphdr *orig_ip = (struct iphdr *)original_packet;
    size_t ip_hdr_len = (orig_ip->ihl) * 4;
    struct tcphdr *orig_tcp = (struct tcphdr *)(original_packet + ip_hdr_len);
    
    // Create response packet
    size_t total_len = 20 + 20 + http_size;  // IP + TCP + HTTP
    if (total_len > max_output) return 0;
    
    memset(output, 0, total_len);
    
    // IP header (swap src/dst)
    struct iphdr *ip = (struct iphdr *)output;
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = htons(total_len);
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_TCP;
    ip->saddr = orig_ip->daddr;  // Swap
    ip->daddr = orig_ip->saddr;  // Swap
    ip->check = 0;  // Will be calculated by kernel
    
    // TCP header (swap ports)
    struct tcphdr *tcp = (struct tcphdr *)(output + 20);
    tcp->source = orig_tcp->dest;  // Swap
    tcp->dest = orig_tcp->source;  // Swap
    tcp->seq = htonl(1);
    tcp->ack_seq = htonl(ntohl(orig_tcp->seq) + 1);
    tcp->doff = 5;
    tcp->psh = 1;
    tcp->ack = 1;
    tcp->window = htons(65535);
    tcp->check = 0;
    tcp->urg_ptr = 0;
    
    // HTTP response
    memcpy(output + 40, http_response, http_size);
    
    return total_len;
}

/**
 * Process IP packet with HTTP request and return HTTP response packet
 */
int http_proxy_process(const uint8_t *ip_packet, size_t packet_size,
                       uint8_t *response_packet, size_t *response_size,
                       size_t max_response) {
    char host[256];
    char http_request[2048];
    
    // Extract HTTP request
    if (extract_http_request(ip_packet, packet_size, host, sizeof(host),
                             http_request, sizeof(http_request)) < 0) {
        fprintf(stderr, "[HTTP Proxy] Failed to extract HTTP request\n");
        return -1;
    }
    
    printf("[HTTP Proxy] Request to %s\n", host);
    
    // Perform HTTP request
    uint8_t http_response[MAX_HTTP_RESPONSE];
    size_t http_response_size;
    
    if (perform_http_request(host, http_request, http_response,
                             &http_response_size, sizeof(http_response)) < 0) {
        return -1;
    }
    
    // Create response packet
    *response_size = create_response_packet(ip_packet, http_response, http_response_size,
                                           response_packet, max_response);
    
    if (*response_size == 0) {
        fprintf(stderr, "[HTTP Proxy] Failed to create response packet\n");
        return -1;
    }
    
    return 0;
}
