#ifndef POST_DATA_H
#define POST_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "timestamp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define MAX_URL_LENGTH 256
#define MAX_HEADER_LENGTH 128
#define MAX_RESPONSE_LENGTH 1024
#define HTTP_TIMEOUT_MS 10000

// HTTP client configuration structure
typedef struct {
    char url[MAX_URL_LENGTH];              // POST endpoint URL
    char auth_header[MAX_HEADER_LENGTH];   // Authorization header (optional)
    char content_type[MAX_HEADER_LENGTH];  // Content-Type header
    int timeout_ms;                        // HTTP timeout in milliseconds
    bool use_https;                        // Use HTTPS (true) or HTTP (false)
} http_config_t;

// POST data manager structure
typedef struct {
    http_config_t config;                  // HTTP configuration
    timestamp_manager_t *ts_manager;       // Reference to timestamp manager
    bool initialized;                      // Initialization status
    char response_buffer[MAX_RESPONSE_LENGTH]; // Buffer for HTTP responses
} post_data_manager_t;

// Function declarations

/**
 * @brief Initialize the POST data manager
 * @param manager Pointer to POST data manager structure
 * @param ts_manager Pointer to timestamp manager
 * @param config HTTP configuration
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_init(post_data_manager_t *manager, 
                         timestamp_manager_t *ts_manager,
                         const http_config_t *config);

/**
 * @brief Set HTTP configuration
 * @param manager Pointer to POST data manager
 * @param url POST endpoint URL
 * @param content_type Content-Type header (e.g., "application/json")
 * @param auth_header Authorization header (can be NULL)
 * @param use_https Use HTTPS if true, HTTP if false
 * @return ESP_OK on success
 */
esp_err_t post_data_set_config(post_data_manager_t *manager,
                               const char *url,
                               const char *content_type,
                               const char *auth_header,
                               bool use_https);

/**
 * @brief Check if SO buffer threshold is reached and send data if needed
 * @param manager Pointer to POST data manager
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_check_and_send(post_data_manager_t *manager);

/**
 * @brief Send packets from SO buffer via HTTP POST
 * @param manager Pointer to POST data manager
 * @param max_packets Maximum number of packets to send in one request
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_send_packets(post_data_manager_t *manager, size_t max_packets);

/**
 * @brief Convert packet to JSON format
 * @param packet Pointer to packet structure
 * @param json_buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_packet_to_json(const packet_t *packet, char *json_buffer, size_t buffer_size);

/**
 * @brief Convert multiple packets to JSON array format
 * @param packets Array of packets
 * @param packet_count Number of packets
 * @param json_buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_packets_to_json(const packet_t *packets, size_t packet_count,
                                    char *json_buffer, size_t buffer_size);

/**
 * @brief Get last HTTP response
 * @param manager Pointer to POST data manager
 * @return Pointer to response buffer
 */
const char* post_data_get_last_response(post_data_manager_t *manager);

/**
 * @brief Force send all packets in SO buffer (for testing or shutdown)
 * @param manager Pointer to POST data manager
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t post_data_flush_all(post_data_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif // POST_DATA_H