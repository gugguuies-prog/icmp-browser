/**
 * statistics.c
 * 
 * Network Statistics Implementation
 */

#include "statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Create statistics context
 */
statistics_t* stats_create(void) {
    statistics_t *stats = (statistics_t*)malloc(sizeof(statistics_t));
    if (!stats) {
        return NULL;
    }
    
    memset(stats, 0, sizeof(statistics_t));
    
    stats->start_time = time(NULL);
    stats->rtt_min = UINT32_MAX;
    stats->rtt_max = 0;
    
    if (mutex_init(&stats->lock) != 0) {
        free(stats);
        return NULL;
    }
    
    return stats;
}

/**
 * Record packet sent
 */
int stats_packet_sent(statistics_t *stats, size_t bytes_sent) {
    if (!stats) {
        return STATS_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&stats->lock);
    
    stats->packets_sent++;
    stats->bytes_sent += bytes_sent;
    stats->last_packet_time = time(NULL);
    
    mutex_unlock(&stats->lock);
    
    return STATS_SUCCESS;
}

/**
 * Record packet received with RTT
 */
int stats_packet_received(statistics_t *stats, size_t bytes_received, uint32_t rtt_ms) {
    if (!stats) {
        return STATS_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&stats->lock);
    
    stats->packets_received++;
    stats->bytes_received += bytes_received;
    stats->last_packet_time = time(NULL);
    
    // Update RTT statistics
    if (rtt_ms < stats->rtt_min) {
        stats->rtt_min = rtt_ms;
    }
    if (rtt_ms > stats->rtt_max) {
        stats->rtt_max = rtt_ms;
    }
    stats->rtt_sum += rtt_ms;
    stats->rtt_count++;
    
    // Reset consecutive losses on successful receive
    stats->consecutive_losses = 0;
    
    mutex_unlock(&stats->lock);
    
    return STATS_SUCCESS;
}

/**
 * Record packet lost
 */
int stats_packet_lost(statistics_t *stats) {
    if (!stats) {
        return STATS_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&stats->lock);
    
    stats->packets_lost++;
    stats->consecutive_losses++;
    
    if (stats->consecutive_losses > stats->max_consecutive_losses) {
        stats->max_consecutive_losses = stats->consecutive_losses;
    }
    
    mutex_unlock(&stats->lock);
    
    return STATS_SUCCESS;
}

/**
 * Record packet retransmitted
 */
int stats_packet_retransmitted(statistics_t *stats, size_t bytes_retransmitted) {
    if (!stats) {
        return STATS_ERROR_INVALID_PARAM;
    }
    
    mutex_lock(&stats->lock);
    
    stats->packets_retransmitted++;
    stats->bytes_sent += bytes_retransmitted;
    
    mutex_unlock(&stats->lock);
    
    return STATS_SUCCESS;
}

/**
 * Get total packets sent
 */
uint64_t stats_get_packets_sent(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    uint64_t count = stats->packets_sent;
    mutex_unlock(&stats->lock);
    
    return count;
}

/**
 * Get total packets received
 */
uint64_t stats_get_packets_received(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    uint64_t count = stats->packets_received;
    mutex_unlock(&stats->lock);
    
    return count;
}

/**
 * Get packet loss count
 */
uint64_t stats_get_packets_lost(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    uint64_t count = stats->packets_lost;
    mutex_unlock(&stats->lock);
    
    return count;
}

/**
 * Get packet loss rate
 */
double stats_get_packet_loss_rate(statistics_t *stats) {
    if (!stats) {
        return 0.0;
    }
    
    mutex_lock(&stats->lock);
    
    uint64_t total = stats->packets_sent;
    uint64_t lost = stats->packets_lost;
    
    mutex_unlock(&stats->lock);
    
    if (total == 0) {
        return 0.0;
    }
    
    return (double)(lost * 100) / (double)total;
}

/**
 * Get average RTT
 */
uint32_t stats_get_average_rtt(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    
    uint32_t avg = 0;
    if (stats->rtt_count > 0) {
        avg = (uint32_t)(stats->rtt_sum / stats->rtt_count);
    }
    
    mutex_unlock(&stats->lock);
    
    return avg;
}

/**
 * Get minimum RTT
 */
uint32_t stats_get_min_rtt(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    uint32_t min = (stats->rtt_min == UINT32_MAX) ? 0 : stats->rtt_min;
    mutex_unlock(&stats->lock);
    
    return min;
}

/**
 * Get maximum RTT
 */
uint32_t stats_get_max_rtt(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    uint32_t max = stats->rtt_max;
    mutex_unlock(&stats->lock);
    
    return max;
}

/**
 * Get throughput (upload)
 */
uint64_t stats_get_throughput_upload(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    
    time_t now = time(NULL);
    time_t elapsed = now - stats->start_time;
    
    uint64_t throughput = 0;
    if (elapsed > 0) {
        throughput = stats->bytes_sent / elapsed;
    }
    
    mutex_unlock(&stats->lock);
    
    return throughput;
}

/**
 * Get throughput (download)
 */
uint64_t stats_get_throughput_download(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    
    time_t now = time(NULL);
    time_t elapsed = now - stats->start_time;
    
    uint64_t throughput = 0;
    if (elapsed > 0) {
        throughput = stats->bytes_received / elapsed;
    }
    
    mutex_unlock(&stats->lock);
    
    return throughput;
}

/**
 * Get connection uptime
 */
uint64_t stats_get_uptime(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    time_t now = time(NULL);
    uint64_t uptime = now - stats->start_time;
    mutex_unlock(&stats->lock);
    
    return uptime;
}

/**
 * Get connection quality score
 */
int stats_get_connection_quality(statistics_t *stats) {
    if (!stats) {
        return 0;
    }
    
    mutex_lock(&stats->lock);
    
    double loss_rate = 0.0;
    if (stats->packets_sent > 0) {
        loss_rate = (double)(stats->packets_lost * 100) / (double)stats->packets_sent;
    }
    
    uint32_t avg_rtt = 0;
    if (stats->rtt_count > 0) {
        avg_rtt = (uint32_t)(stats->rtt_sum / stats->rtt_count);
    }
    
    uint32_t consecutive = stats->max_consecutive_losses;
    
    mutex_unlock(&stats->lock);
    
    // Calculate quality score (0-100)
    int quality = 100;
    
    // Loss rate penalty (max -50 points)
    quality -= (int)(loss_rate * 0.5);
    
    // RTT penalty (>100ms = -20, >200ms = -40, >500ms = -60)
    if (avg_rtt > 500) {
        quality -= 60;
    } else if (avg_rtt > 200) {
        quality -= 40;
    } else if (avg_rtt > 100) {
        quality -= 20;
    }
    
    // Consecutive losses penalty (>5 = -20, >10 = -40)
    if (consecutive > 10) {
        quality -= 40;
    } else if (consecutive > 5) {
        quality -= 20;
    }
    
    // Clamp to 0-100
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;
    
    return quality;
}

/**
 * Reset statistics
 */
void stats_reset(statistics_t *stats) {
    if (!stats) {
        return;
    }
    
    mutex_lock(&stats->lock);
    
    stats->packets_sent = 0;
    stats->packets_received = 0;
    stats->packets_lost = 0;
    stats->packets_retransmitted = 0;
    stats->bytes_sent = 0;
    stats->bytes_received = 0;
    stats->rtt_min = UINT32_MAX;
    stats->rtt_max = 0;
    stats->rtt_sum = 0;
    stats->rtt_count = 0;
    stats->start_time = time(NULL);
    stats->consecutive_losses = 0;
    stats->max_consecutive_losses = 0;
    
    mutex_unlock(&stats->lock);
}

/**
 * Print statistics summary
 */
void stats_print(statistics_t *stats) {
    if (!stats) {
        return;
    }
    
    mutex_lock(&stats->lock);
    
    uint64_t uptime = time(NULL) - stats->start_time;
    double loss_rate = 0.0;
    if (stats->packets_sent > 0) {
        loss_rate = (double)(stats->packets_lost * 100) / (double)stats->packets_sent;
    }
    
    uint32_t avg_rtt = 0;
    if (stats->rtt_count > 0) {
        avg_rtt = (uint32_t)(stats->rtt_sum / stats->rtt_count);
    }
    
    uint64_t upload = 0, download = 0;
    if (uptime > 0) {
        upload = stats->bytes_sent / uptime;
        download = stats->bytes_received / uptime;
    }
    
    mutex_unlock(&stats->lock);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           Network Statistics                               ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Uptime:        %llu seconds                                \n", 
           (unsigned long long)uptime);
    printf("║                                                            ║\n");
    printf("║ Packets:                                                   ║\n");
    printf("║   Sent:        %llu                                        \n",
           (unsigned long long)stats->packets_sent);
    printf("║   Received:    %llu                                        \n",
           (unsigned long long)stats->packets_received);
    printf("║   Lost:        %llu (%.2f%%)                               \n",
           (unsigned long long)stats->packets_lost, loss_rate);
    printf("║   Retrans:     %llu                                        \n",
           (unsigned long long)stats->packets_retransmitted);
    printf("║                                                            ║\n");
    printf("║ RTT:                                                       ║\n");
    printf("║   Min:         %u ms                                       \n",
           stats->rtt_min == UINT32_MAX ? 0 : stats->rtt_min);
    printf("║   Avg:         %u ms                                       \n", avg_rtt);
    printf("║   Max:         %u ms                                       \n", stats->rtt_max);
    printf("║                                                            ║\n");
    printf("║ Throughput:                                                ║\n");
    printf("║   Upload:      %llu bytes/s                                \n",
           (unsigned long long)upload);
    printf("║   Download:    %llu bytes/s                                \n",
           (unsigned long long)download);
    printf("║                                                            ║\n");
    printf("║ Quality:       %d/100                                      \n",
           stats_get_connection_quality(stats));
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/**
 * Destroy statistics context
 */
void stats_destroy(statistics_t *stats) {
    if (!stats) {
        return;
    }
    
    mutex_destroy(&stats->lock);
    free(stats);
}
