#include "post_data.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "POST_DATA";

// HTTP event handler
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    post_data_manager_t *manager = (post_data_manager_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (manager && evt->data_len < MAX_RESPONSE_LENGTH - 1) {
                strncat(manager->response_buffer, (char*)evt->data, evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

esp_err_t post_data_init(post_data_manager_t *manager, 
                         timestamp_manager_t *ts_manager,
                         const http_config_t *config)
{
    if (!manager || !ts_manager || !config) {
        ESP_LOGE(TAG, "Invalid parameters for POST data manager initialization");
        return ESP_FAIL;
    }

    // Copy configuration
    memcpy(&manager->config, config, sizeof(http_config_t));
    manager->ts_manager = ts_manager;
    manager->initialized = true;
    memset(manager->response_buffer, 0, MAX_RESPONSE_LENGTH);

    ESP_LOGI(TAG, "POST data manager initialized with URL: %s", config->url);
    return ESP_OK;
}

esp_err_t post_data_set_config(post_data_manager_t *manager,
                               const char *url,
                               const char *content_type,
                               const char *auth_header,
                               bool use_https)
{
    if (!manager || !url || !content_type) {
        ESP_LOGE(TAG, "Invalid parameters for setting config");
        return ESP_FAIL;
    }

    strncpy(manager->config.url, url, MAX_URL_LENGTH - 1);
    manager->config.url[MAX_URL_LENGTH - 1] = '\0';

    strncpy(manager->config.content_type, content_type, MAX_HEADER_LENGTH - 1);
    manager->config.content_type[MAX_HEADER_LENGTH - 1] = '\0';

    if (auth_header) {
        strncpy(manager->config.auth_header, auth_header, MAX_HEADER_LENGTH - 1);
        manager->config.auth_header[MAX_HEADER_LENGTH - 1] = '\0';
    } else {
        manager->config.auth_header[0] = '\0';
    }

    manager->config.use_https = use_https;
    manager->config.timeout_ms = HTTP_TIMEOUT_MS;

    ESP_LOGI(TAG, "HTTP config updated: URL=%s, HTTPS=%s", url, use_https ? "true" : "false");
    return ESP_OK;
}

esp_err_t post_data_check_and_send(post_data_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        ESP_LOGE(TAG, "POST data manager not initialized");
        return ESP_FAIL;
    }

    // Check if SO buffer threshold is reached
    if (timestamp_so_buffer_ready(manager->ts_manager)) {
        ESP_LOGI(TAG, "SO buffer threshold reached, sending data");
        return post_data_send_packets(manager, 50); // Send up to 50 packets at once
    }

    return ESP_OK;
}

esp_err_t post_data_packet_to_json(const packet_t *packet, char *json_buffer, size_t buffer_size)
{
    if (!packet || !json_buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for packet to JSON conversion");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }

    // Add timestamp information
    cJSON_AddNumberToObject(json, "epoch_time", (double)packet->epoch_time);
    cJSON_AddNumberToObject(json, "esp_timer_us", (double)packet->esp_timer_us);
    cJSON_AddBoolToObject(json, "has_epoch", packet->has_epoch);

    // Convert packet data to base64 or hex string for JSON transmission
    char *data_hex = malloc(packet->data_len * 2 + 1);
    if (!data_hex) {
        ESP_LOGE(TAG, "Failed to allocate memory for hex conversion");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < packet->data_len; i++) {
        sprintf(data_hex + i * 2, "%02x", packet->data[i]);
    }
    data_hex[packet->data_len * 2] = '\0';

    cJSON_AddStringToObject(json, "data", data_hex);
    cJSON_AddNumberToObject(json, "data_len", (double)packet->data_len);

    char *json_string = cJSON_Print(json);
    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print JSON");
        free(data_hex);
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    size_t json_len = strlen(json_string);
    if (json_len >= buffer_size) {
        ESP_LOGE(TAG, "JSON string too large for buffer");
        free(data_hex);
        free(json_string);
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    strcpy(json_buffer, json_string);

    free(data_hex);
    free(json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t post_data_packets_to_json(const packet_t *packets, size_t packet_count,
                                    char *json_buffer, size_t buffer_size)
{
    if (!packets || !json_buffer || packet_count == 0 || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters for packets to JSON conversion");
        return ESP_FAIL;
    }

    cJSON *json_array = cJSON_CreateArray();
    if (!json_array) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        return ESP_FAIL;
    }

    for (size_t i = 0; i < packet_count; i++) {
        cJSON *packet_json = cJSON_CreateObject();
        if (!packet_json) {
            ESP_LOGE(TAG, "Failed to create packet JSON object");
            cJSON_Delete(json_array);
            return ESP_FAIL;
        }

        // Add packet data to JSON object
        cJSON_AddNumberToObject(packet_json, "epoch_time", (double)packets[i].epoch_time);
        cJSON_AddNumberToObject(packet_json, "esp_timer_us", (double)packets[i].esp_timer_us);
        cJSON_AddBoolToObject(packet_json, "has_epoch", packets[i].has_epoch);

        // Convert packet data to hex string
        char *data_hex = malloc(packets[i].data_len * 2 + 1);
        if (!data_hex) {
            ESP_LOGE(TAG, "Failed to allocate memory for hex conversion");
            cJSON_Delete(packet_json);
            cJSON_Delete(json_array);
            return ESP_FAIL;
        }

        for (size_t j = 0; j < packets[i].data_len; j++) {
            sprintf(data_hex + j * 2, "%02x", packets[i].data[j]);
        }
        data_hex[packets[i].data_len * 2] = '\0';

        cJSON_AddStringToObject(packet_json, "data", data_hex);
        cJSON_AddNumberToObject(packet_json, "data_len", (double)packets[i].data_len);

        cJSON_AddItemToArray(json_array, packet_json);
        free(data_hex);
    }

    // Create root object with metadata
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "packet_count", (double)packet_count);
    cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
    cJSON_AddItemToObject(root, "packets", json_array);

    char *json_string = cJSON_Print(root);
    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print JSON array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    size_t json_len = strlen(json_string);
    if (json_len >= buffer_size) {
        ESP_LOGE(TAG, "JSON string too large for buffer");
        free(json_string);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    strcpy(json_buffer, json_string);

    free(json_string);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t post_data_send_packets(post_data_manager_t *manager, size_t max_packets)
{
    if (!manager || !manager->initialized) {
        ESP_LOGE(TAG, "POST data manager not initialized");
        return ESP_FAIL;
    }

    // Get packets from SO buffer
    packet_t *packets = malloc(max_packets * sizeof(packet_t));
    if (!packets) {
        ESP_LOGE(TAG, "Failed to allocate memory for packets");
        return ESP_FAIL;
    }

    size_t actual_count = 0;
    esp_err_t ret = timestamp_get_so_packets(manager->ts_manager, packets, max_packets, &actual_count);
    if (ret != ESP_OK || actual_count == 0) {
        ESP_LOGW(TAG, "No packets to send or error getting packets");
        free(packets);
        return ESP_OK; // Not an error if no packets to send
    }

    // Convert packets to JSON
    char *json_buffer = malloc(32768); // 32KB buffer for JSON
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
        free(packets);
        return ESP_FAIL;
    }

    ret = post_data_packets_to_json(packets, actual_count, json_buffer, 32768);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to convert packets to JSON");
        free(packets);
        free(json_buffer);
        return ESP_FAIL;
    }

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = manager->config.url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = manager->config.timeout_ms,
        .event_handler = http_event_handler,
        .user_data = manager,
        .is_async = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(packets);
        free(json_buffer);
        return ESP_FAIL;
    }

    // Set headers
    esp_http_client_set_header(client, "Content-Type", manager->config.content_type);
    if (strlen(manager->config.auth_header) > 0) {
        esp_http_client_set_header(client, "Authorization", manager->config.auth_header);
    }

    // Clear response buffer
    memset(manager->response_buffer, 0, MAX_RESPONSE_LENGTH);

    // Set POST data
    esp_http_client_set_post_field(client, json_buffer, strlen(json_buffer));

    // Perform HTTP POST
    ESP_LOGI(TAG, "Sending %zu packets via HTTP POST", actual_count);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);

        if (status_code >= 200 && status_code < 300) {
            // Success - remove packets from SO buffer
            timestamp_remove_so_packets(manager->ts_manager, actual_count);
            ESP_LOGI(TAG, "Successfully sent and removed %zu packets", actual_count);
            ret = ESP_OK;
        } else {
            ESP_LOGE(TAG, "HTTP POST failed with status code: %d", status_code);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        ret = ESP_FAIL;
    }

    // Cleanup
    esp_http_client_cleanup(client);
    free(packets);
    free(json_buffer);

    return ret;
}

const char* post_data_get_last_response(post_data_manager_t *manager)
{
    if (!manager) {
        return NULL;
    }
    return manager->response_buffer;
}

esp_err_t post_data_flush_all(post_data_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        ESP_LOGE(TAG, "POST data manager not initialized");
        return ESP_FAIL;
    }

    size_t so_count = 0;
    timestamp_get_stats(manager->ts_manager, NULL, &so_count);

    if (so_count == 0) {
        ESP_LOGI(TAG, "No packets to flush");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Flushing all %zu packets from SO buffer", so_count);

    // Send all packets in batches
    esp_err_t ret = ESP_OK;
    while (so_count > 0) {
        size_t batch_size = (so_count > 50) ? 50 : so_count;
        esp_err_t batch_ret = post_data_send_packets(manager, batch_size);
        
        if (batch_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send batch of %zu packets", batch_size);
            ret = ESP_FAIL;
            break;
        }

        timestamp_get_stats(manager->ts_manager, NULL, &so_count);
    }

    return ret;
}