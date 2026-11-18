/**
 * statistics.h
 * 
 * Network Statistics and Monitoring
 * 
 * Features:
 * - Packet counters (sent/received/lost)
 * - RTT measurements (min/max/avg)
 * - Throughput calculation (bytes/sec)
 * - Bandwidth usage tracking
 * - Connection quality metrics
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>

// Error codes
#define STATS_SUCCESS 0
#define STATS_ERROR_INVALID_PARAM -1

/**
 * Statistics context
 */
typedef struct {
    // Packet counters
    uint64_t packets_sent;               // Total packets sent
    uint64_t packets_received;           // Total packets received
    uint64_t packets_lost;               // Packets lost/timeout
    uint64_t packets_retransmitted;      // Retransmitted packets
    
    // Byte counters
    uint64_t bytes_sent;                 // Total bytes sent
    uint64_t bytes_received;             // Total bytes received
    
    // RTT statistics (in milliseconds)
    uint32_t rtt_min;                    // Minimum RTT
    uint32_t rtt_max;                    // Maximum RTT
    uint64_t rtt_sum;                    // Sum for average calculation
    uint64_t rtt_count;                  // Number of RTT samples
    
    // Throughput tracking
    time_t start_time;                   // Session start time
    time_t last_packet_time;             // Last packet timestamp
    
    // Connection quality
    uint32_t consecutive_losses;         // Consecutive packet losses
    uint32_t max_consecutive_losses;     // Max consecutive losses seen
    
    // Thread safety
    pthread_mutex_t lock;
    
} statistics_t;

/**
 * Create statistics context
 * 
 * @return Pointer to statistics_t on success, NULL on error
 * 
 * Example:
 *   statistics_t *stats = stats_create();
 *   if (stats) {
 *       // Use stats
 *       stats_destroy(stats);
 *   }
 */
statistics_t* stats_create(void);

/**
 * Record packet sent
 * 
 * @param stats Statistics context
 * @param bytes_sent Number of bytes sent
 * 
 * @return STATS_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   stats_packet_sent(stats, 1472);  // Sent 1472 byte packet
 */
int stats_packet_sent(statistics_t *stats, size_t bytes_sent);

/**
 * Record packet received with RTT
 * 
 * @param stats Statistics context
 * @param bytes_received Number of bytes received
 * @param rtt_ms Round-trip time in milliseconds
 * 
 * @return STATS_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   stats_packet_received(stats, 1400, 25);  // Received packet, RTT 25ms
 */
int stats_packet_received(statistics_t *stats, size_t bytes_received, uint32_t rtt_ms);

/**
 * Record packet lost
 * 
 * @param stats Statistics context
 * 
 * @return STATS_SUCCESS on success, error code otherwise
 * 
 * Example:
 *   stats_packet_lost(stats);  // Packet timeout or lost
 */
int stats_packet_lost(statistics_t *stats);

/**
 * Record packet retransmitted
 * 
 * @param stats Statistics context
 * @param bytes_retransmitted Number of bytes retransmitted
 * 
 * @return STATS_SUCCESS on success, error code otherwise
 */
int stats_packet_retransmitted(statistics_t *stats, size_t bytes_retransmitted);

/**
 * Get total packets sent
 * 
 * @param stats Statistics context
 * @return Number of packets sent
 */
uint64_t stats_get_packets_sent(statistics_t *stats);

/**
 * Get total packets received
 * 
 * @param stats Statistics context
 * @return Number of packets received
 */
uint64_t stats_get_packets_received(statistics_t *stats);

/**
 * Get packet loss count
 * 
 * @param stats Statistics context
 * @return Number of packets lost
 */
uint64_t stats_get_packets_lost(statistics_t *stats);

/**
 * Get packet loss rate
 * 
 * @param stats Statistics context
 * @return Loss rate as percentage (0.0 - 100.0)
 * 
 * Example:
 *   double loss = stats_get_packet_loss_rate(stats);
 *   printf("Packet loss: %.2f%%\n", loss);
 */
double stats_get_packet_loss_rate(statistics_t *stats);

/**
 * Get average RTT
 * 
 * @param stats Statistics context
 * @return Average RTT in milliseconds, 0 if no samples
 * 
 * Example:
 *   uint32_t avg_rtt = stats_get_average_rtt(stats);
 *   printf("Average RTT: %u ms\n", avg_rtt);
 */
uint32_t stats_get_average_rtt(statistics_t *stats);

/**
 * Get minimum RTT
 * 
 * @param stats Statistics context
 * @return Minimum RTT in milliseconds
 */
uint32_t stats_get_min_rtt(statistics_t *stats);

/**
 * Get maximum RTT
 * 
 * @param stats Statistics context
 * @return Maximum RTT in milliseconds
 */
uint32_t stats_get_max_rtt(statistics_t *stats);

/**
 * Get throughput (upload)
 * 
 * @param stats Statistics context
 * @return Upload throughput in bytes per second
 * 
 * Example:
 *   uint64_t upload = stats_get_throughput_upload(stats);
 *   printf("Upload: %llu KB/s\n", upload / 1024);
 */
uint64_t stats_get_throughput_upload(statistics_t *stats);

/**
 * Get throughput (download)
 * 
 * @param stats Statistics context
 * @return Download throughput in bytes per second
 */
uint64_t stats_get_throughput_download(statistics_t *stats);

/**
 * Get connection uptime
 * 
 * @param stats Statistics context
 * @return Uptime in seconds
 */
uint64_t stats_get_uptime(statistics_t *stats);

/**
 * Get connection quality score
 * 
 * @param stats Statistics context
 * @return Quality score (0-100), 100 = excellent, 0 = terrible
 * 
 * Note: Based on loss rate, RTT, and consecutive losses
 * 
 * Example:
 *   int quality = stats_get_connection_quality(stats);
 *   if (quality >= 80) printf("Excellent\n");
 *   else if (quality >= 60) printf("Good\n");
 *   else if (quality >= 40) printf("Fair\n");
 *   else printf("Poor\n");
 */
int stats_get_connection_quality(statistics_t *stats);

/**
 * Reset statistics
 * 
 * @param stats Statistics context
 * 
 * Example:
 *   stats_reset(stats);  // Clear all counters
 */
void stats_reset(statistics_t *stats);

/**
 * Print statistics summary
 * 
 * @param stats Statistics context
 * 
 * Example:
 *   stats_print(stats);
 */
void stats_print(statistics_t *stats);

/**
 * Destroy statistics context
 * 
 * @param stats Statistics context
 */
void stats_destroy(statistics_t *stats);

#endif // STATISTICS_H
