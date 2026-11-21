/**
 * http_proxy.c
 * 
 * HTTP/HTTPS proxy for ICMP tunnel server
 * Extracts HTTP requests from IP packets and performs real HTTP/HTTPS requests
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
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_HTTP_RESPONSE 65536

// Global SSL context
static SSL_CTX *g_ssl_ctx = NULL;

/**
 * Initialize SSL
 */
static void init_ssl(void) {
    if (g_ssl_ctx) return;
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    const SSL_METHOD *method = TLS_client_method();
    g_ssl_ctx = SSL_CTX_new(method);
    
    if (!g_ssl_ctx) {
        fprintf(stderr, "[HTTP Proxy] Failed to create SSL context\n");
        ERR_print_errors_fp(stderr);
        return;
    }
    
    // Set options for compatibility
    SSL_CTX_set_options(g_ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_NONE, NULL);  // Skip cert verification for now
    
    printf("[HTTP Proxy] SSL initialized\n");
}

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
 * Perform HTTP request (plain)
 */
static int perform_http_request(const char *host, int port, const char *http_request,
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
    server_addr.sin_port = htons(port);
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
    
    printf("[HTTP Proxy] HTTP: Received %zu bytes from %s\n", total_received, host);
    
    return 0;
}

/**
 * Perform HTTPS request (with SSL)
 */
static int perform_https_request(const char *host, int port, const char *http_request,
                                 uint8_t *response, size_t *response_size, size_t max_size) {
    // Initialize SSL if needed
    if (!g_ssl_ctx) {
        init_ssl();
        if (!g_ssl_ctx) return -1;
    }
    
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
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[HTTP Proxy] connect");
        close(sock);
        return -1;
    }
    
    // Create SSL connection
    SSL *ssl = SSL_new(g_ssl_ctx);
    if (!ssl) {
        fprintf(stderr, "[HTTP Proxy] Failed to create SSL\n");
        close(sock);
        return -1;
    }
    
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host);  // SNI
    
    if (SSL_connect(ssl) <= 0) {
        fprintf(stderr, "[HTTP Proxy] SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        return -1;
    }
    
    // Send request
    size_t req_len = strlen(http_request);
    if (SSL_write(ssl, http_request, req_len) <= 0) {
        fprintf(stderr, "[HTTP Proxy] SSL write failed\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sock);
        return -1;
    }
    
    // Receive response
    size_t total_received = 0;
    int n;
    
        n = SSL_read(ssl, response + total_received, max_size - total_received - 1);
        if (n <= 0) break;
        total_received += n;
    }
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    
    if (total_received == 0) {
        fprintf(stderr, "[HTTP Proxy] No response received\n");
        return -1;
    }
    
    response[total_received] = '\0';
    *response_size = total_received;
    
    printf("[HTTP Proxy] HTTPS: Received %zu bytes from %s\n", total_received, host);
    
    return 0;
}

/**
 * Follow HTTP redirects and perform request
 */
static int perform_http_with_redirects(const char *initial_host, const char *initial_request,
                                       uint8_t *response, size_t *response_size, size_t max_size,
                                       int max_redirects) {
    char host[256];
    char request[2048];
    int use_https = 0;
    int port = 80;
    
    strncpy(host, initial_host, sizeof(host) - 1);
    strncpy(request, initial_request, sizeof(request) - 1);
    
    for (int redirect_count = 0; redirect_count <= max_redirects; redirect_count++) {
        uint8_t temp_response[MAX_HTTP_RESPONSE];
        size_t temp_size;
        int result;
        
        // Perform request (HTTP or HTTPS)
        if (use_https) {
            result = perform_https_request(host, port, request, temp_response, &temp_size, sizeof(temp_response));
        } else {
            result = perform_http_request(host, port, request, temp_response, &temp_size, sizeof(temp_response));
        }
        
        if (result < 0) return -1;
        
        // Check for redirects (301, 302, 303, 307, 308)
        if (strstr((char*)temp_response, "HTTP/1.1 301") ||
            strstr((char*)temp_response, "HTTP/1.0 301") ||
            strstr((char*)temp_response, "HTTP/1.1 302") ||
            strstr((char*)temp_response, "HTTP/1.0 302") ||
            strstr((char*)temp_response, "HTTP/1.1 303") ||
            strstr((char*)temp_response, "HTTP/1.1 307") ||
            strstr((char*)temp_response, "HTTP/1.1 308")) {
            
            // Extract Location header
            char *location = strstr((char*)temp_response, "Location: ");
            if (!location) location = strstr((char*)temp_response, "location: ");
            
            if (location && redirect_count < max_redirects) {
                location += 10;  // Skip "Location: "
                char *end = strstr(location, "\r\n");
                if (!end) end = strstr(location, "\n");
                
                if (end) {
                    char new_url[512];
                    size_t url_len = end - location;
                    if (url_len > sizeof(new_url) - 1) url_len = sizeof(new_url) - 1;
                    memcpy(new_url, location, url_len);
                    new_url[url_len] = '\0';
                    
                    printf("[HTTP Proxy] Redirect to: %s\n", new_url);
                    
                    // Parse new URL
                    if (strncmp(new_url, "https://", 8) == 0) {
                        use_https = 1;
                        port = 443;
                        sscanf(new_url + 8, "%255[^/]", host);
                    } else if (strncmp(new_url, "http://", 7) == 0) {
                        use_https = 0;
                        port = 80;
                        sscanf(new_url + 7, "%255[^/]", host);
                    } else {
                        // Relative redirect - keep same host/protocol
                    }
                    
                    // Update request for new URL
                    char *path = strchr(new_url + (use_https ? 8 : 7), '/');
                    if (!path) path = "/";
                    
                    snprintf(request, sizeof(request),
                             "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                             path, host);
                    
                    continue;  // Follow redirect
                }
            }
        }
        
        // No redirect or final response - return it
        if (temp_size > max_size) temp_size = max_size;
        memcpy(response, temp_response, temp_size);
        *response_size = temp_size;
        return 0;
    }
    
    // Too many redirects
    fprintf(stderr, "[HTTP Proxy] Too many redirects\n");
    return -1;
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
    
    // Check if this is raw HTTP (starts with GET, POST, etc.) instead of IP packet
    if (packet_size > 4 && 
        (memcmp(ip_packet, "GET ", 4) == 0 || 
         memcmp(ip_packet, "POST", 4) == 0 ||
         memcmp(ip_packet, "HEAD", 4) == 0 ||
         memcmp(ip_packet, "PUT ", 4) == 0)) {
        
        // Raw HTTP request - extract host and use as-is
        if (packet_size >= sizeof(http_request)) {
            fprintf(stderr, "[HTTP Proxy] HTTP request too large\n");
            return -1;
        }
        
        memcpy(http_request, ip_packet, packet_size);
        http_request[packet_size] = '\0';
        
        // Extract Host header
        char *host_line = strstr(http_request, "Host: ");
        if (!host_line) host_line = strstr(http_request, "host: ");
        
        if (!host_line) {
            fprintf(stderr, "[HTTP Proxy] No Host header found\n");
            return -1;
        }
        
        host_line += 6;  // Skip "Host: "
        char *end = strstr(host_line, "\r\n");
        if (!end) end = strstr(host_line, "\n");
        
        if (end) {
            size_t len = end - host_line;
            if (len > sizeof(host) - 1) len = sizeof(host) - 1;
            memcpy(host, host_line, len);
            host[len] = '\0';
        } else {
            fprintf(stderr, "[HTTP Proxy] Malformed Host header\n");
            return -1;
        }
        
        printf("[HTTP Proxy] Raw HTTP request to %s\n", host);
        
    } else {
        // IP packet - extract HTTP from TCP payload
        if (extract_http_request(ip_packet, packet_size, host, sizeof(host),
                                 http_request, sizeof(http_request)) < 0) {
            fprintf(stderr, "[HTTP Proxy] Failed to extract HTTP request\n");
            return -1;
        }
        
        printf("[HTTP Proxy] Request to %s\n", host);
    }
    
    // Perform HTTP/HTTPS request with redirect following
    uint8_t http_response[MAX_HTTP_RESPONSE];
    size_t http_response_size;
    
    if (perform_http_with_redirects(host, http_request, http_response,
                                    &http_response_size, sizeof(http_response), 5) < 0) {
        return -1;
    }
    
    // Return raw HTTP response (not IP packet)
    // Limit response to 1200 bytes to fit in single ICMP packet (Windows limitation)
    if (http_response_size > max_response) {
        http_response_size = max_response;
    }
    memcpy(response_packet, http_response, http_response_size);
    *response_size = http_response_size;
    
    return 0;
}
