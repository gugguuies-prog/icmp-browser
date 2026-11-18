/**
 * http_proxy.c
 * 
 * HTTP Proxy Server Implementation
 */

#include "http_proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define close closesocket
#define SHUT_RDWR SD_BOTH
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#include <errno.h>
#endif

// Forward declarations
static void* accept_thread_func(void *arg);
static void* client_handler_thread(void *arg);

/**
 * Initialize Windows sockets (Windows only)
 */
static int init_winsock(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    return 0;
#endif
}

/**
 * Create HTTP proxy server
 */
http_proxy_t* http_proxy_create(uint16_t port, icmp_session_t *icmp_session) {
    // icmp_session can be NULL for testing purposes
    
    // Initialize Winsock
    if (init_winsock() != 0) {
        return NULL;
    }
    
    http_proxy_t *proxy = (http_proxy_t*)malloc(sizeof(http_proxy_t));
    if (!proxy) {
        return NULL;
    }
    
    memset(proxy, 0, sizeof(http_proxy_t));
    
    proxy->port = (port == 0) ? HTTP_PROXY_DEFAULT_PORT : port;
    proxy->icmp_session = icmp_session;
    proxy->max_clients = HTTP_PROXY_MAX_CONNECTIONS;
    proxy->next_connection_id = 1;
    proxy->listen_socket = -1;
    proxy->running = 0;
    
    // Allocate clients array
    proxy->clients = (http_client_t*)calloc(proxy->max_clients, sizeof(http_client_t));
    if (!proxy->clients) {
        free(proxy);
        return NULL;
    }
    
    // Initialize mutexes
    if (mutex_init(&proxy->clients_lock) != 0) {
        free(proxy->clients);
        free(proxy);
        return NULL;
    }
    
    if (mutex_init(&proxy->stats_lock) != 0) {
        mutex_destroy(&proxy->clients_lock);
        free(proxy->clients);
        free(proxy);
        return NULL;
    }
    
    return proxy;
}

/**
 * Start proxy server
 */
int http_proxy_start(http_proxy_t *proxy) {
    if (!proxy || proxy->running) {
        return HTTP_PROXY_ERROR_INVALID_PARAM;
    }
    
    // Create listening socket
    proxy->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (proxy->listen_socket < 0) {
        return HTTP_PROXY_ERROR_SOCKET;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(proxy->listen_socket, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&reuse, sizeof(reuse));
    
    // Bind to address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(HTTP_PROXY_BIND_ADDRESS);
    addr.sin_port = htons(proxy->port);
    
    if (bind(proxy->listen_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(proxy->listen_socket);
        return HTTP_PROXY_ERROR_BIND;
    }
    
    // Listen for connections
    if (listen(proxy->listen_socket, 5) < 0) {
        close(proxy->listen_socket);
        return HTTP_PROXY_ERROR_LISTEN;
    }
    
    // Start accept thread
    proxy->running = 1;
    if (thread_create(&proxy->accept_thread, (thread_func_t)accept_thread_func, proxy) != 0) {
        proxy->running = 0;
        close(proxy->listen_socket);
        return HTTP_PROXY_ERROR_THREAD;
    }
    
    printf("[HTTP Proxy] Listening on %s:%d\n", HTTP_PROXY_BIND_ADDRESS, proxy->port);
    
    return HTTP_PROXY_SUCCESS;
}

/**
 * Stop proxy server
 */
void http_proxy_stop(http_proxy_t *proxy) {
    if (!proxy || !proxy->running) {
        return;
    }
    
    proxy->running = 0;
    
    // Close listening socket to unblock accept()
    if (proxy->listen_socket >= 0) {
        shutdown(proxy->listen_socket, SHUT_RDWR);
        close(proxy->listen_socket);
        proxy->listen_socket = -1;
    }
    
    // Wait for accept thread
    thread_join(proxy->accept_thread);
    
    // Close all client connections
    mutex_lock(&proxy->clients_lock);
    for (size_t i = 0; i < proxy->max_clients; i++) {
        if (proxy->clients[i].active) {
            if (proxy->clients[i].socket_fd >= 0) {
                shutdown(proxy->clients[i].socket_fd, SHUT_RDWR);
                close(proxy->clients[i].socket_fd);
            }
            thread_join(proxy->clients[i].thread);
            proxy->clients[i].active = 0;
        }
    }
    mutex_unlock(&proxy->clients_lock);
    
    printf("[HTTP Proxy] Stopped\n");
}

/**
 * Destroy proxy server
 */
void http_proxy_destroy(http_proxy_t *proxy) {
    if (!proxy) {
        return;
    }
    
    http_proxy_stop(proxy);
    
    mutex_destroy(&proxy->clients_lock);
    mutex_destroy(&proxy->stats_lock);
    
    free(proxy->clients);
    free(proxy);
    
#ifdef _WIN32
    WSACleanup();
#endif
}

/**
 * Accept thread - accepts new client connections
 */
static void* accept_thread_func(void *arg) {
    http_proxy_t *proxy = (http_proxy_t*)arg;
    
    while (proxy->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(proxy->listen_socket, 
                                   (struct sockaddr*)&client_addr, &addr_len);
        
        if (client_socket < 0) {
            if (proxy->running) {
                // Only log if we're still supposed to be running
                printf("[HTTP Proxy] Accept error\n");
            }
            continue;
        }
        
        // Find free slot for client
        mutex_lock(&proxy->clients_lock);
        
        http_client_t *client = NULL;
        for (size_t i = 0; i < proxy->max_clients; i++) {
            if (!proxy->clients[i].active) {
                client = &proxy->clients[i];
                break;
            }
        }
        
        if (!client) {
            // No free slots
            mutex_unlock(&proxy->clients_lock);
            close(client_socket);
            printf("[HTTP Proxy] Max connections reached, rejected client\n");
            continue;
        }
        
        // Initialize client
        client->socket_fd = client_socket;
        client->addr = client_addr;
        client->active = 1;
        client->connection_id = proxy->next_connection_id++;
        client->last_activity = time(NULL);
        client->proxy = proxy;  // Set proxy pointer
        
        // Start client handler thread
        if (thread_create(&client->thread, client_handler_thread, client) != 0) {
            close(client_socket);
            client->active = 0;
            mutex_unlock(&proxy->clients_lock);
            printf("[HTTP Proxy] Failed to create client thread\n");
            continue;
        }
        
        mutex_unlock(&proxy->clients_lock);
        
        printf("[HTTP Proxy] Client connected: %s (ID %u)\n",
               inet_ntoa(client_addr.sin_addr), client->connection_id);
    }
    
    return NULL;
}

/**
 * Client handler thread - handles individual client connection
 */
static void* client_handler_thread(void *arg) {
    http_client_t *client = (http_client_t*)arg;
    http_proxy_t *proxy = client->proxy;  // Get proxy from client
    
    uint8_t buffer[HTTP_PROXY_BUFFER_SIZE];
    
    while (client->active) {
        // Receive HTTP request
        int bytes_received = recv(client->socket_fd, (char*)buffer, 
                                 sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            // Connection closed or error
            break;
        }
        
        buffer[bytes_received] = '\0';
        client->last_activity = time(NULL);
        
        printf("[HTTP Proxy] Received %d bytes from client %u\n",
               bytes_received, client->connection_id);
        
        // TODO: Parse and handle HTTP request
        // TODO: Forward through ICMP tunnel
        // TODO: Receive response from tunnel
        // TODO: Send response to client
        
        // Update statistics
        mutex_lock(&proxy->stats_lock);
        proxy->total_requests++;
        proxy->total_bytes_received += bytes_received;
        mutex_unlock(&proxy->stats_lock);
        
        // For now, send a simple response
        const char *response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 48\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body>ICMP Tunnel Active</body></html>";
        
        send(client->socket_fd, response, (int)strlen(response), 0);
        
        // Update statistics
        mutex_lock(&proxy->stats_lock);
        proxy->total_bytes_sent += strlen(response);
        mutex_unlock(&proxy->stats_lock);
        
        break; // Close connection after response
    }
    
    // Close client socket
    shutdown(client->socket_fd, SHUT_RDWR);
    close(client->socket_fd);
    client->active = 0;
    
    printf("[HTTP Proxy] Client disconnected (ID %u)\n", client->connection_id);
    
    return NULL;
}

/**
 * Parse HTTP request
 */
int http_parse_request(const uint8_t *buffer, size_t size, http_request_t *request) {
    if (!buffer || !request || size == 0) {
        return HTTP_PROXY_ERROR_INVALID_PARAM;
    }
    
    memset(request, 0, sizeof(http_request_t));
    
    // Find end of request line
    const char *line_end = strstr((const char*)buffer, "\r\n");
    if (!line_end) {
        return HTTP_PROXY_ERROR_PARSE;
    }
    
    // Parse request line: METHOD URI VERSION
    int parsed = sscanf((const char*)buffer, "%15s %2047s %15s",
                       request->method, request->uri, request->version);
    if (parsed != 3) {
        return HTTP_PROXY_ERROR_PARSE;
    }
    
    // Find headers start
    const char *headers_start = line_end + 2;
    const char *headers_end = strstr(headers_start, "\r\n\r\n");
    
    if (!headers_end) {
        // No body, headers end at buffer end
        headers_end = (const char*)buffer + size;
    }
    
    // Parse headers
    const char *current = headers_start;
    while (current < headers_end) {
        const char *next_line = strstr(current, "\r\n");
        if (!next_line) {
            break;
        }
        
        // Check for Host header
        if (strncasecmp(current, "Host:", 5) == 0) {
            sscanf(current + 5, " %255s", request->host);
        }
        // Check for Content-Length
        else if (strncasecmp(current, "Content-Length:", 15) == 0) {
            sscanf(current + 15, " %d", &request->content_length);
        }
        // Check for Connection
        else if (strncasecmp(current, "Connection:", 11) == 0) {
            char conn[32];
            sscanf(current + 11, " %31s", conn);
            if (strcasecmp(conn, "keep-alive") == 0) {
                request->keep_alive = 1;
            }
        }
        
        current = next_line + 2;
    }
    
    // Set default port
    request->port = 80;
    
    // Check for port in Host header
    char *port_sep = strchr(request->host, ':');
    if (port_sep) {
        *port_sep = '\0';
        request->port = atoi(port_sep + 1);
    }
    
    return HTTP_PROXY_SUCCESS;
}

/**
 * Build HTTP response
 */
int http_build_response(const http_response_t *response, uint8_t *buffer,
                        size_t buffer_size, size_t *written) {
    if (!response || !buffer || !written) {
        return HTTP_PROXY_ERROR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    
    // Status line
    int n = snprintf((char*)buffer + offset, buffer_size - offset,
                    "%s %d %s\r\n",
                    response->version, response->status_code, response->status_text);
    if (n < 0 || (size_t)n >= buffer_size - offset) {
        return HTTP_PROXY_ERROR_SEND;
    }
    offset += n;
    
    // Headers
    if (response->headers && response->headers_len > 0) {
        if (offset + response->headers_len >= buffer_size) {
            return HTTP_PROXY_ERROR_SEND;
        }
        memcpy(buffer + offset, response->headers, response->headers_len);
        offset += response->headers_len;
    }
    
    // End of headers
    if (offset + 2 >= buffer_size) {
        return HTTP_PROXY_ERROR_SEND;
    }
    memcpy(buffer + offset, "\r\n", 2);
    offset += 2;
    
    // Body
    if (response->body && response->body_len > 0) {
        if (offset + response->body_len >= buffer_size) {
            return HTTP_PROXY_ERROR_SEND;
        }
        memcpy(buffer + offset, response->body, response->body_len);
        offset += response->body_len;
    }
    
    *written = offset;
    return HTTP_PROXY_SUCCESS;
}

/**
 * Free HTTP request
 */
void http_free_request(http_request_t *request) {
    if (!request) {
        return;
    }
    
    if (request->headers) {
        free(request->headers);
        request->headers = NULL;
    }
    
    if (request->body) {
        free(request->body);
        request->body = NULL;
    }
}

/**
 * Free HTTP response
 */
void http_free_response(http_response_t *response) {
    if (!response) {
        return;
    }
    
    if (response->headers) {
        free(response->headers);
        response->headers = NULL;
    }
    
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }
}

/**
 * Get proxy statistics
 */
void http_proxy_get_stats(http_proxy_t *proxy, uint64_t *total_requests,
                          uint64_t *total_bytes_sent, uint64_t *total_bytes_received,
                          int *active_connections) {
    if (!proxy) {
        return;
    }
    
    mutex_lock(&proxy->stats_lock);
    if (total_requests) *total_requests = proxy->total_requests;
    if (total_bytes_sent) *total_bytes_sent = proxy->total_bytes_sent;
    if (total_bytes_received) *total_bytes_received = proxy->total_bytes_received;
    mutex_unlock(&proxy->stats_lock);
    
    if (active_connections) {
        mutex_lock(&proxy->clients_lock);
        int count = 0;
        for (size_t i = 0; i < proxy->max_clients; i++) {
            if (proxy->clients[i].active) {
                count++;
            }
        }
        *active_connections = count;
        mutex_unlock(&proxy->clients_lock);
    }
}

/**
 * Print proxy statistics
 */
void http_proxy_print_stats(http_proxy_t *proxy) {
    if (!proxy) {
        return;
    }
    
    uint64_t requests, bytes_sent, bytes_received;
    int active;
    
    http_proxy_get_stats(proxy, &requests, &bytes_sent, &bytes_received, &active);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           HTTP Proxy Statistics                            ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Active Connections:  %d                                    \n", active);
    printf("║ Total Requests:      %llu                                  \n",
           (unsigned long long)requests);
    printf("║ Bytes Sent:          %llu                                  \n",
           (unsigned long long)bytes_sent);
    printf("║ Bytes Received:      %llu                                  \n",
           (unsigned long long)bytes_received);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}
