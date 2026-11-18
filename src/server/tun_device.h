/**
 * tun_device.h
 * 
 * TUN Virtual Network Interface Management
 * 
 * Features:
 * - Create/destroy TUN device
 * - Configure IP address and netmask
 * - Read/write IP packets
 * - Non-blocking I/O support
 * - MTU configuration
 */

#ifndef TUN_DEVICE_H
#define TUN_DEVICE_H

#ifndef __linux__
#error "TUN device is Linux-only"
#endif

#include <stdint.h>
#include <stddef.h>

// Constants
#define TUN_MTU 1500                     // Maximum Transfer Unit
#define TUN_BUFFER_SIZE 2048             // Buffer size for packet I/O
#define TUN_DEFAULT_NAME "tun0"          // Default interface name

// Error codes
#define TUN_SUCCESS 0
#define TUN_ERROR_INVALID_PARAM -1
#define TUN_ERROR_OPEN_FAILED -2
#define TUN_ERROR_IOCTL_FAILED -3
#define TUN_ERROR_CONFIG_FAILED -4
#define TUN_ERROR_READ_FAILED -5
#define TUN_ERROR_WRITE_FAILED -6
#define TUN_ERROR_TIMEOUT -7
#define TUN_ERROR_PERMISSION -8

/**
 * TUN device context
 */
typedef struct {
    int fd;                              // File descriptor
    char name[16];                       // Interface name (e.g., "tun0")
    uint8_t is_open;                     // Device open flag
    char ip_addr[16];                    // IP address (e.g., "10.0.1.1")
    char netmask[16];                    // Netmask (e.g., "255.255.255.0")
    int mtu;                             // MTU size
} tun_device_t;

/**
 * Create and open TUN device
 * 
 * @param device_name Interface name (e.g., "tun0", NULL for auto)
 * 
 * @return Pointer to tun_device_t on success, NULL on error
 * 
 * Note: Requires root privileges or CAP_NET_ADMIN capability
 * 
 * Example:
 *   tun_device_t *tun = tun_create("tun0");
 *   if (tun) {
 *       // Use device
 *       tun_destroy(tun);
 *   }
 */
tun_device_t* tun_create(const char *device_name);

/**
 * Configure TUN device IP address and netmask
 * 
 * @param tun TUN device context
 * @param ip_addr IP address string (e.g., "10.0.1.1")
 * @param netmask Netmask string (e.g., "255.255.255.0")
 * 
 * @return TUN_SUCCESS on success, error code otherwise
 * 
 * Note: Brings interface UP automatically
 * 
 * Example:
 *   tun_device_t *tun = tun_create("tun0");
 *   tun_configure(tun, "10.0.1.1", "255.255.255.0");
 */
int tun_configure(tun_device_t *tun, const char *ip_addr, const char *netmask);

/**
 * Set TUN device MTU
 * 
 * @param tun TUN device context
 * @param mtu MTU size (typically 1500)
 * 
 * @return TUN_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   tun_set_mtu(tun, 1500);
 */
int tun_set_mtu(tun_device_t *tun, int mtu);

/**
 * Write IP packet to TUN device
 * 
 * @param tun TUN device context
 * @param packet IP packet data
 * @param packet_size Size of packet in bytes
 * 
 * @return Number of bytes written, or error code (< 0)
 * 
 * Note: Packet must be a valid IP packet (starts with 0x45 for IPv4)
 * 
 * Example:
 *   uint8_t ip_packet[100];  // Valid IP packet
 *   int written = tun_write(tun, ip_packet, 100);
 *   if (written < 0) {
 *       printf("Write error: %s\n", tun_error_string(written));
 *   }
 */
int tun_write(tun_device_t *tun, const uint8_t *packet, size_t packet_size);

/**
 * Read IP packet from TUN device (blocking)
 * 
 * @param tun TUN device context
 * @param buffer Buffer to store packet
 * @param buffer_size Size of buffer
 * 
 * @return Number of bytes read, or error code (< 0)
 * 
 * Note: Blocks until packet available
 * 
 * Example:
 *   uint8_t buffer[TUN_BUFFER_SIZE];
 *   int bytes = tun_read(tun, buffer, sizeof(buffer));
 *   if (bytes > 0) {
 *       printf("Received %d byte IP packet\n", bytes);
 *   }
 */
int tun_read(tun_device_t *tun, uint8_t *buffer, size_t buffer_size);

/**
 * Read IP packet from TUN device with timeout
 * 
 * @param tun TUN device context
 * @param buffer Buffer to store packet
 * @param buffer_size Size of buffer
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * 
 * @return Number of bytes read, TUN_ERROR_TIMEOUT if timeout, or error code (< 0)
 * 
 * Example:
 *   uint8_t buffer[TUN_BUFFER_SIZE];
 *   int bytes = tun_read_timeout(tun, buffer, sizeof(buffer), 1000);
 *   if (bytes == TUN_ERROR_TIMEOUT) {
 *       printf("No packet received within 1 second\n");
 *   } else if (bytes > 0) {
 *       printf("Received %d byte IP packet\n", bytes);
 *   }
 */
int tun_read_timeout(tun_device_t *tun, uint8_t *buffer, size_t buffer_size, 
                      int timeout_ms);

/**
 * Get TUN device file descriptor
 * 
 * @param tun TUN device context
 * 
 * @return File descriptor, or -1 if device not open
 * 
 * Example:
 *   int fd = tun_get_fd(tun);
 *   // Use fd with select() or poll()
 */
int tun_get_fd(tun_device_t *tun);

/**
 * Get TUN device name
 * 
 * @param tun TUN device context
 * 
 * @return Device name string (e.g., "tun0")
 * 
 * Example:
 *   printf("Device name: %s\n", tun_get_name(tun));
 */
const char* tun_get_name(tun_device_t *tun);

/**
 * Check if packet is valid IPv4
 * 
 * @param packet Packet data
 * @param packet_size Packet size
 * 
 * @return 1 if valid IPv4, 0 otherwise
 * 
 * Note: Checks version (4), header length, and total length
 * 
 * Example:
 *   if (tun_is_valid_ipv4(buffer, bytes)) {
 *       printf("Valid IPv4 packet\n");
 *   }
 */
int tun_is_valid_ipv4(const uint8_t *packet, size_t packet_size);

/**
 * Close and destroy TUN device
 * 
 * @param tun TUN device context
 * 
 * Note: Frees memory and closes file descriptor
 * 
 * Example:
 *   tun_destroy(tun);
 */
void tun_destroy(tun_device_t *tun);

/**
 * Get error message string
 * 
 * @param error_code Error code from TUN functions
 * @return Human-readable error message
 */
const char* tun_error_string(int error_code);

#endif // TUN_DEVICE_H
