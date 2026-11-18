/**
 * test_http_proxy.c
 * 
 * Unit tests for HTTP Proxy
 */

#include "../src/client/http_proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define sleep(x) Sleep((x)*1000)
#define close closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name) \
    do { \
        printf("\n=== Test %d: %s ===\n", ++tests_run, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("✓ PASSED\n"); \
        tests_passed++; \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("✗ FAILED: %s\n", msg); \
            printf("  Line %d: %s\n", __LINE__, #cond); \
            return; \
        } \
    } while(0)

/**
 * Helper: Connect to proxy
 */
static int connect_to_proxy(uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

/**
 * Test 1: Parse HTTP GET Request
 */
void test_parse_get_request(void) {
    TEST_START("Parse HTTP GET Request");
    
    const char *request_str = 
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: ICMPBrowser/1.0\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    http_request_t request;
    int result = http_parse_request((const uint8_t*)request_str, 
                                   strlen(request_str), &request);
    
    ASSERT(result == HTTP_PROXY_SUCCESS, "Parse should succeed");
    ASSERT(strcmp(request.method, "GET") == 0, "Method should be GET");
    ASSERT(strcmp(request.uri, "/index.html") == 0, "URI should be /index.html");
    ASSERT(strcmp(request.version, "HTTP/1.1") == 0, "Version should be HTTP/1.1");
    ASSERT(strcmp(request.host, "example.com") == 0, "Host should be example.com");
    ASSERT(request.port == 80, "Port should be 80 (default)");
    ASSERT(request.keep_alive == 1, "Keep-alive should be set");
    
    printf("Parsed: %s %s %s (Host: %s:%d)\n",
           request.method, request.uri, request.version, request.host, request.port);
    
    http_free_request(&request);
    
    TEST_PASS();
}

/**
 * Test 2: Parse HTTP POST Request
 */
void test_parse_post_request(void) {
    TEST_START("Parse HTTP POST Request");
    
    const char *request_str = 
        "POST /api/data HTTP/1.1\r\n"
        "Host: api.example.com:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 25\r\n"
        "\r\n"
        "{\"key\":\"value\",\"id\":1}";
    
    http_request_t request;
    int result = http_parse_request((const uint8_t*)request_str,
                                   strlen(request_str), &request);
    
    ASSERT(result == HTTP_PROXY_SUCCESS, "Parse should succeed");
    ASSERT(strcmp(request.method, "POST") == 0, "Method should be POST");
    ASSERT(strcmp(request.uri, "/api/data") == 0, "URI should be /api/data");
    ASSERT(strcmp(request.host, "api.example.com") == 0, "Host should be api.example.com");
    ASSERT(request.port == 8080, "Port should be 8080");
    ASSERT(request.content_length == 25, "Content-Length should be 25");
    
    printf("Parsed: %s %s (Host: %s:%d, Content-Length: %d)\n",
           request.method, request.uri, request.host, request.port, 
           request.content_length);
    
    http_free_request(&request);
    
    TEST_PASS();
}

/**
 * Test 3: Build HTTP Response
 */
void test_build_response(void) {
    TEST_START("Build HTTP Response");
    
    http_response_t response;
    memset(&response, 0, sizeof(response));
    
    strcpy(response.version, "HTTP/1.1");
    response.status_code = 200;
    strcpy(response.status_text, "OK");
    
    const char *body = "<html><body>Hello</body></html>";
    response.body = (uint8_t*)body;
    response.body_len = strlen(body);
    
    char headers[256];
    snprintf(headers, sizeof(headers),
            "Content-Type: text/html\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close",
            (unsigned long)response.body_len);
    response.headers = headers;
    response.headers_len = strlen(headers);
    
    uint8_t buffer[1024];
    size_t written;
    
    int result = http_build_response(&response, buffer, sizeof(buffer), &written);
    
    ASSERT(result == HTTP_PROXY_SUCCESS, "Build should succeed");
    ASSERT(written > 0, "Should write some bytes");
    ASSERT(strstr((const char*)buffer, "HTTP/1.1 200 OK") != NULL, 
           "Should contain status line");
    ASSERT(strstr((const char*)buffer, "Content-Type: text/html") != NULL,
           "Should contain Content-Type");
    ASSERT(strstr((const char*)buffer, "<html>") != NULL,
           "Should contain body");
    
    printf("Built response: %lu bytes\n", (unsigned long)written);
    printf("Response preview:\n%.*s\n", (int)(written > 200 ? 200 : written), buffer);
    
    TEST_PASS();
}

/**
 * Test 4: Create and Destroy Proxy (no ICMP session needed for this test)
 */
void test_create_destroy(void) {
    TEST_START("Create and Destroy Proxy");
    
    // Pass NULL for ICMP session (not needed for basic create/destroy)
    http_proxy_t *proxy = http_proxy_create(8888, NULL);
    ASSERT(proxy != NULL, "Proxy creation should succeed");
    ASSERT(proxy->port == 8888, "Port should be 8888");
    ASSERT(proxy->max_clients == HTTP_PROXY_MAX_CONNECTIONS,
           "Max clients should be set");
    
    printf("Created proxy on port %d (max %lu clients)\n",
           proxy->port, (unsigned long)proxy->max_clients);
    
    http_proxy_destroy(proxy);
    
    TEST_PASS();
}

/**
 * Test 5: Start and Stop Proxy
 */
void test_start_stop(void) {
    TEST_START("Start and Stop Proxy");
    
    http_proxy_t *proxy = http_proxy_create(9999, NULL);
    ASSERT(proxy != NULL, "Proxy creation should succeed");
    
    int result = http_proxy_start(proxy);
    ASSERT(result == HTTP_PROXY_SUCCESS, "Proxy start should succeed");
    ASSERT(proxy->running == 1, "Proxy should be running");
    
    printf("Proxy started on port %d\n", proxy->port);
    
    // Wait a bit
    sleep(1);
    
    http_proxy_stop(proxy);
    ASSERT(proxy->running == 0, "Proxy should be stopped");
    
    printf("Proxy stopped\n");
    
    http_proxy_destroy(proxy);
    
    TEST_PASS();
}

/**
 * Test 6: Accept Connection
 */
void test_accept_connection(void) {
    TEST_START("Accept Connection");
    
    http_proxy_t *proxy = http_proxy_create(10000, NULL);
    ASSERT(proxy != NULL, "Proxy creation should succeed");
    
    int result = http_proxy_start(proxy);
    ASSERT(result == HTTP_PROXY_SUCCESS, "Proxy start should succeed");
    
    // Wait for proxy to start listening
    sleep(1);
    
    // Connect to proxy
    int client_sock = connect_to_proxy(10000);
    ASSERT(client_sock >= 0, "Connection should succeed");
    
    printf("Connected to proxy\n");
    
    // Send a simple HTTP request
    const char *request = 
        "GET / HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "\r\n";
    
    int sent = send(client_sock, request, (int)strlen(request), 0);
    ASSERT(sent == (int)strlen(request), "Should send full request");
    
    printf("Sent %d bytes\n", sent);
    
    // Receive response
    char response[1024];
    int received = recv(client_sock, response, sizeof(response) - 1, 0);
    ASSERT(received > 0, "Should receive response");
    
    response[received] = '\0';
    printf("Received %d bytes:\n%s\n", received, response);
    
    ASSERT(strstr(response, "HTTP/1.1 200 OK") != NULL,
           "Should receive HTTP 200 OK");
    
    close(client_sock);
    
    http_proxy_stop(proxy);
    http_proxy_destroy(proxy);
    
    TEST_PASS();
}

/**
 * Test 7: Get Statistics
 */
void test_get_statistics(void) {
    TEST_START("Get Statistics");
    
    http_proxy_t *proxy = http_proxy_create(8888, NULL);
    ASSERT(proxy != NULL, "Proxy creation should succeed");
    
    uint64_t requests, bytes_sent, bytes_received;
    int active_connections;
    
    http_proxy_get_stats(proxy, &requests, &bytes_sent, &bytes_received,
                        &active_connections);
    
    ASSERT(requests == 0, "Initial requests should be 0");
    ASSERT(bytes_sent == 0, "Initial bytes sent should be 0");
    ASSERT(bytes_received == 0, "Initial bytes received should be 0");
    ASSERT(active_connections == 0, "Initial active connections should be 0");
    
    printf("Initial stats: requests=%llu, sent=%llu, received=%llu, active=%d\n",
           (unsigned long long)requests, (unsigned long long)bytes_sent,
           (unsigned long long)bytes_received, active_connections);
    
    http_proxy_print_stats(proxy);
    
    http_proxy_destroy(proxy);
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║            HTTP Proxy Unit Tests                           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    test_parse_get_request();
    test_parse_post_request();
    test_build_response();
    test_create_destroy();
    test_start_stop();
    test_accept_connection();
    test_get_statistics();
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║ TEST SUMMARY                                               ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total tests: %2d                                            ║\n", tests_run);
    printf("║ Passed:      %2d                                            ║\n", tests_passed);
    printf("║ Failed:      %2d                                            ║\n", tests_run - tests_passed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    if (tests_passed == tests_run) {
        printf("\n🎉 ALL TESTS PASSED! 🎉\n\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED ❌\n\n");
        return 1;
    }
}
