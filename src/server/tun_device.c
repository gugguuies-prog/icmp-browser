/**
 * tun_device.c
 * 
 * TUN Virtual Network Interface Implementation
 */

#include "tun_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <errno.h>

/**
 * Create and open TUN device
 */
tun_device_t* tun_create(const char *device_name) {
    // Allocate context
    tun_device_t *tun = (tun_device_t*)malloc(sizeof(tun_device_t));
    if (!tun) {
        return NULL;
    }
    
    memset(tun, 0, sizeof(tun_device_t));
    
    // Open TUN device
    tun->fd = open("/dev/net/tun", O_RDWR);
    if (tun->fd < 0) {
        free(tun);
        return NULL;
    }
    
    // Configure TUN interface
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    
    // TUN mode (layer 3, IP packets)
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    
    // Set device name
    if (device_name) {
        strncpy(ifr.ifr_name, device_name, IFNAMSIZ - 1);
        strncpy(tun->name, device_name, sizeof(tun->name) - 1);
    } else {
        strncpy(tun->name, TUN_DEFAULT_NAME, sizeof(tun->name) - 1);
    }
    
    // Create interface
    if (ioctl(tun->fd, TUNSETIFF, (void*)&ifr) < 0) {
        close(tun->fd);
        free(tun);
        return NULL;
    }
    
    // Store actual name (kernel may have changed it)
    strncpy(tun->name, ifr.ifr_name, sizeof(tun->name) - 1);
    tun->is_open = 1;
    tun->mtu = TUN_MTU;
    
    return tun;
}

/**
 * Configure TUN device IP address and netmask
 */
int tun_configure(tun_device_t *tun, const char *ip_addr, const char *netmask) {
    if (!tun || !tun->is_open || !ip_addr || !netmask) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    // Store configuration
    strncpy(tun->ip_addr, ip_addr, sizeof(tun->ip_addr) - 1);
    strncpy(tun->netmask, netmask, sizeof(tun->netmask) - 1);
    
    // Create socket for ioctl
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    struct ifreq ifr;
    struct sockaddr_in *addr;
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, tun->name, IFNAMSIZ - 1);
    
    // Set IP address
    addr = (struct sockaddr_in*)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_addr, &addr->sin_addr) != 1) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    // Set netmask
    addr = (struct sockaddr_in*)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, netmask, &addr->sin_addr) != 1) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    // Bring interface UP
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    close(sock);
    return TUN_SUCCESS;
}

/**
 * Set TUN device MTU
 */
int tun_set_mtu(tun_device_t *tun, int mtu) {
    if (!tun || !tun->is_open || mtu <= 0 || mtu > 65535) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, tun->name, IFNAMSIZ - 1);
    ifr.ifr_mtu = mtu;
    
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        close(sock);
        return TUN_ERROR_CONFIG_FAILED;
    }
    
    close(sock);
    tun->mtu = mtu;
    
    return TUN_SUCCESS;
}

/**
 * Write IP packet to TUN device
 */
int tun_write(tun_device_t *tun, const uint8_t *packet, size_t packet_size) {
    if (!tun || !tun->is_open || !packet || packet_size == 0) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    if (packet_size > TUN_MTU) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    ssize_t written = write(tun->fd, packet, packet_size);
    if (written < 0) {
        return TUN_ERROR_WRITE_FAILED;
    }
    
    return (int)written;
}

/**
 * Read IP packet from TUN device (blocking)
 */
int tun_read(tun_device_t *tun, uint8_t *buffer, size_t buffer_size) {
    if (!tun || !tun->is_open || !buffer || buffer_size == 0) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    ssize_t bytes_read = read(tun->fd, buffer, buffer_size);
    if (bytes_read < 0) {
        return TUN_ERROR_READ_FAILED;
    }
    
    return (int)bytes_read;
}

/**
 * Read IP packet from TUN device with timeout
 */
int tun_read_timeout(tun_device_t *tun, uint8_t *buffer, size_t buffer_size, 
                      int timeout_ms) {
    if (!tun || !tun->is_open || !buffer || buffer_size == 0) {
        return TUN_ERROR_INVALID_PARAM;
    }
    
    // Setup select() for timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(tun->fd, &readfds);
    
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int ret = select(tun->fd + 1, &readfds, NULL, NULL, 
                     timeout_ms >= 0 ? &tv : NULL);
    
    if (ret < 0) {
        return TUN_ERROR_READ_FAILED;
    } else if (ret == 0) {
        return TUN_ERROR_TIMEOUT;
    }
    
    // Data available, read it
    return tun_read(tun, buffer, buffer_size);
}

/**
 * Get TUN device file descriptor
 */
int tun_get_fd(tun_device_t *tun) {
    if (!tun || !tun->is_open) {
        return -1;
    }
    return tun->fd;
}

/**
 * Get TUN device name
 */
const char* tun_get_name(tun_device_t *tun) {
    if (!tun) {
        return NULL;
    }
    return tun->name;
}

/**
 * Check if packet is valid IPv4
 */
int tun_is_valid_ipv4(const uint8_t *packet, size_t packet_size) {
    if (!packet || packet_size < 20) {
        return 0;
    }
    
    // Check version (upper 4 bits should be 4)
    uint8_t version = (packet[0] >> 4) & 0x0F;
    if (version != 4) {
        return 0;
    }
    
    // Check header length (lower 4 bits, in 32-bit words)
    uint8_t ihl = packet[0] & 0x0F;
    if (ihl < 5 || ihl * 4 > packet_size) {
        return 0;
    }
    
    // Check total length
    uint16_t total_length = (packet[2] << 8) | packet[3];
    if (total_length < 20 || total_length > packet_size) {
        return 0;
    }
    
    return 1;
}

/**
 * Close and destroy TUN device
 */
void tun_destroy(tun_device_t *tun) {
    if (!tun) {
        return;
    }
    
    if (tun->is_open && tun->fd >= 0) {
        close(tun->fd);
    }
    
    tun->is_open = 0;
    tun->fd = -1;
    
    free(tun);
}

/**
 * Get error message string
 */
const char* tun_error_string(int error_code) {
    switch (error_code) {
        case TUN_SUCCESS:
            return "Success";
        case TUN_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case TUN_ERROR_OPEN_FAILED:
            return "Failed to open /dev/net/tun";
        case TUN_ERROR_IOCTL_FAILED:
            return "ioctl() failed";
        case TUN_ERROR_CONFIG_FAILED:
            return "Configuration failed";
        case TUN_ERROR_READ_FAILED:
            return "Read failed";
        case TUN_ERROR_WRITE_FAILED:
            return "Write failed";
        case TUN_ERROR_TIMEOUT:
            return "Timeout";
        case TUN_ERROR_PERMISSION:
            return "Permission denied (requires root)";
        default:
            return "Unknown error";
    }
}
