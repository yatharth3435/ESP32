#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define RT_BUFFER_SIZE 100      // Real-time buffer size (before SNTP sync)
#define SO_BUFFER_SIZE 200      // Send-out buffer size (after timestamping)
#define MAX_PACKET_SIZE 512     // Maximum size of a single packet

// Packet structure for buffering
typedef struct {
    uint8_t data[MAX_PACKET_SIZE];  // Packet data
    size_t data_len;                // Actual data length
    int64_t esp_timer_us;          // ESP timer timestamp in microseconds
    time_t epoch_time;             // Epoch timestamp (0 if not calculated yet)
    bool has_epoch;                // Flag indicating if epoch time is valid
} packet_t;

// Buffer structures
typedef struct {
    packet_t packets[RT_BUFFER_SIZE];
    size_t head;                   // Write index
    size_t tail;                   // Read index
    size_t count;                  // Number of packets in buffer
    bool is_full;                  // Buffer full flag
} rt_buffer_t;

typedef struct {
    packet_t packets[SO_BUFFER_SIZE];
    size_t head;                   // Write index
    size_t tail;                   // Read index
    size_t count;                  // Number of packets in buffer
    bool is_full;                  // Buffer full flag
} so_buffer_t;

// Timestamp manager structure
typedef struct {
    rt_buffer_t rt_buffer;         // Real-time buffer (before SNTP sync)
    so_buffer_t so_buffer;         // Send-out buffer (after timestamping)
    bool sntp_synced;              // SNTP synchronization status
    time_t sntp_sync_time;         // Time when SNTP sync occurred
    int64_t sntp_sync_esp_timer;   // ESP timer value when SNTP sync occurred
    size_t so_buffer_threshold;    // Threshold for sending data
} timestamp_manager_t;

// Function declarations

/**
 * @brief Initialize the timestamp manager
 * @param manager Pointer to timestamp manager structure
 * @param threshold Threshold for SO buffer to trigger data sending
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t timestamp_init(timestamp_manager_t *manager, size_t threshold);

/**
 * @brief Add a packet to the RT buffer (called when packet arrives before SNTP sync)
 * @param manager Pointer to timestamp manager
 * @param data Packet data
 * @param data_len Length of packet data
 * @return ESP_OK on success, ESP_FAIL if buffer is full
 */
esp_err_t timestamp_add_packet(timestamp_manager_t *manager, const uint8_t *data, size_t data_len);

/**
 * @brief Notify that SNTP synchronization has occurred
 * @param manager Pointer to timestamp manager
 * @param sync_time Current epoch time from SNTP
 * @return ESP_OK on success
 */
esp_err_t timestamp_sntp_synced(timestamp_manager_t *manager, time_t sync_time);

/**
 * @brief Perform backward timestamping and move packets from RT to SO buffer
 * @param manager Pointer to timestamp manager
 * @return ESP_OK on success, number of packets processed
 */
esp_err_t timestamp_process_rt_buffer(timestamp_manager_t *manager);

/**
 * @brief Check if SO buffer has reached threshold
 * @param manager Pointer to timestamp manager
 * @return true if threshold reached, false otherwise
 */
bool timestamp_so_buffer_ready(timestamp_manager_t *manager);

/**
 * @brief Get packets from SO buffer for sending
 * @param manager Pointer to timestamp manager
 * @param packets Output array to store packets
 * @param max_packets Maximum number of packets to retrieve
 * @param actual_count Actual number of packets retrieved
 * @return ESP_OK on success
 */
esp_err_t timestamp_get_so_packets(timestamp_manager_t *manager, packet_t *packets, 
                                   size_t max_packets, size_t *actual_count);

/**
 * @brief Remove processed packets from SO buffer
 * @param manager Pointer to timestamp manager
 * @param count Number of packets to remove from front of buffer
 * @return ESP_OK on success
 */
esp_err_t timestamp_remove_so_packets(timestamp_manager_t *manager, size_t count);

/**
 * @brief Get current buffer statistics
 * @param manager Pointer to timestamp manager
 * @param rt_count Output: number of packets in RT buffer
 * @param so_count Output: number of packets in SO buffer
 */
void timestamp_get_stats(timestamp_manager_t *manager, size_t *rt_count, size_t *so_count);

#ifdef __cplusplus
}
#endif

#endif // TIMESTAMP_H