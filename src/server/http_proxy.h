/**
 * http_proxy.h
 * 
 * HTTP proxy for ICMP tunnel server
 */

#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include <stdint.h>
#include <stddef.h>

/**
 * Process IP packet containing HTTP request
 * 
 * @param ip_packet Input IP packet with HTTP request
 * @param packet_size Size of input packet
 * @param response_packet Output buffer for response packet
 * @param response_size Output size of response packet
 * @param max_response Maximum size of response buffer
 * @return 0 on success, -1 on error
 */
int http_proxy_process(const uint8_t *ip_packet, size_t packet_size,
                       uint8_t *response_packet, size_t *response_size,
                       size_t max_response);

#endif // HTTP_PROXY_H
