/**
 * http_utils.h
 * 
 * HTTP utilities for ICMP Browser
 */

#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <string>

/**
 * Create HTTP GET packet encapsulated in IP/TCP
 */
size_t create_http_get_packet(const char* host, const char* path,
                               const char* src_ip, const char* dst_ip,
                               uint8_t* buffer, size_t buffer_size);

/**
 * Parse HTTP response from IP/TCP packet
 * Returns HTTP status code, and fills html with the response body
 */
int parse_http_response(const uint8_t* packet, size_t packet_size, std::string& html);

#endif // HTTP_UTILS_H
