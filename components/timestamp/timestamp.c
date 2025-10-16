#include "timestamp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "TIMESTAMP";

esp_err_t timestamp_init(timestamp_manager_t *manager, size_t threshold)
{
    if (!manager) {
        ESP_LOGE(TAG, "Manager pointer is NULL");
        return ESP_FAIL;
    }

    // Initialize RT buffer
    memset(&manager->rt_buffer, 0, sizeof(rt_buffer_t));
    manager->rt_buffer.head = 0;
    manager->rt_buffer.tail = 0;
    manager->rt_buffer.count = 0;
    manager->rt_buffer.is_full = false;

    // Initialize SO buffer
    memset(&manager->so_buffer, 0, sizeof(so_buffer_t));
    manager->so_buffer.head = 0;
    manager->so_buffer.tail = 0;
    manager->so_buffer.count = 0;
    manager->so_buffer.is_full = false;

    // Initialize manager state
    manager->sntp_synced = false;
    manager->sntp_sync_time = 0;
    manager->sntp_sync_esp_timer = 0;
    manager->so_buffer_threshold = threshold;

    ESP_LOGI(TAG, "Timestamp manager initialized with SO buffer threshold: %zu", threshold);
    return ESP_OK;
}

esp_err_t timestamp_add_packet(timestamp_manager_t *manager, const uint8_t *data, size_t data_len)
{
    if (!manager || !data || data_len == 0 || data_len > MAX_PACKET_SIZE) {
        ESP_LOGE(TAG, "Invalid parameters for adding packet");
        return ESP_FAIL;
    }

    // If SNTP is already synced, we can directly add to SO buffer with current timestamp
    if (manager->sntp_synced) {
        // Calculate current epoch time using SNTP sync reference
        int64_t current_esp_timer = esp_timer_get_time();
        int64_t time_diff_us = current_esp_timer - manager->sntp_sync_esp_timer;
        time_t current_epoch = manager->sntp_sync_time + (time_diff_us / 1000000);

        // Add directly to SO buffer
        if (manager->so_buffer.is_full) {
            ESP_LOGW(TAG, "SO buffer is full, dropping packet");
            return ESP_FAIL;
        }

        packet_t *packet = &manager->so_buffer.packets[manager->so_buffer.head];
        memcpy(packet->data, data, data_len);
        packet->data_len = data_len;
        packet->esp_timer_us = current_esp_timer;
        packet->epoch_time = current_epoch;
        packet->has_epoch = true;

        manager->so_buffer.head = (manager->so_buffer.head + 1) % SO_BUFFER_SIZE;
        manager->so_buffer.count++;
        if (manager->so_buffer.count == SO_BUFFER_SIZE) {
            manager->so_buffer.is_full = true;
        }

        ESP_LOGD(TAG, "Added packet directly to SO buffer (SNTP synced), epoch: %ld", current_epoch);
        return ESP_OK;
    }

    // SNTP not synced yet, add to RT buffer
    if (manager->rt_buffer.is_full) {
        ESP_LOGW(TAG, "RT buffer is full, dropping packet");
        return ESP_FAIL;
    }

    packet_t *packet = &manager->rt_buffer.packets[manager->rt_buffer.head];
    memcpy(packet->data, data, data_len);
    packet->data_len = data_len;
    packet->esp_timer_us = esp_timer_get_time();
    packet->epoch_time = 0;
    packet->has_epoch = false;

    manager->rt_buffer.head = (manager->rt_buffer.head + 1) % RT_BUFFER_SIZE;
    manager->rt_buffer.count++;
    if (manager->rt_buffer.count == RT_BUFFER_SIZE) {
        manager->rt_buffer.is_full = true;
    }

    ESP_LOGD(TAG, "Added packet to RT buffer, esp_timer: %lld", packet->esp_timer_us);
    return ESP_OK;
}

esp_err_t timestamp_sntp_synced(timestamp_manager_t *manager, time_t sync_time)
{
    if (!manager) {
        ESP_LOGE(TAG, "Manager pointer is NULL");
        return ESP_FAIL;
    }

    manager->sntp_synced = true;
    manager->sntp_sync_time = sync_time;
    manager->sntp_sync_esp_timer = esp_timer_get_time();

    ESP_LOGI(TAG, "SNTP synchronized at epoch: %ld, esp_timer: %lld", 
             sync_time, manager->sntp_sync_esp_timer);

    // Process RT buffer immediately after SNTP sync
    return timestamp_process_rt_buffer(manager);
}

esp_err_t timestamp_process_rt_buffer(timestamp_manager_t *manager)
{
    if (!manager || !manager->sntp_synced) {
        ESP_LOGE(TAG, "Cannot process RT buffer: manager NULL or SNTP not synced");
        return ESP_FAIL;
    }

    size_t processed_count = 0;

    ESP_LOGI(TAG, "Processing RT buffer with %zu packets", manager->rt_buffer.count);

    // Process all packets in RT buffer
    while (manager->rt_buffer.count > 0) {
        // Check if SO buffer has space
        if (manager->so_buffer.is_full) {
            ESP_LOGW(TAG, "SO buffer full, cannot process more RT packets");
            break;
        }

        // Get packet from RT buffer tail
        packet_t *rt_packet = &manager->rt_buffer.packets[manager->rt_buffer.tail];
        
        // Calculate backward timestamp
        int64_t time_diff_us = rt_packet->esp_timer_us - manager->sntp_sync_esp_timer;
        time_t backward_epoch = manager->sntp_sync_time + (time_diff_us / 1000000);

        // Add to SO buffer with calculated epoch time
        packet_t *so_packet = &manager->so_buffer.packets[manager->so_buffer.head];
        memcpy(so_packet->data, rt_packet->data, rt_packet->data_len);
        so_packet->data_len = rt_packet->data_len;
        so_packet->esp_timer_us = rt_packet->esp_timer_us;
        so_packet->epoch_time = backward_epoch;
        so_packet->has_epoch = true;

        // Update SO buffer pointers
        manager->so_buffer.head = (manager->so_buffer.head + 1) % SO_BUFFER_SIZE;
        manager->so_buffer.count++;
        if (manager->so_buffer.count == SO_BUFFER_SIZE) {
            manager->so_buffer.is_full = true;
        }

        // Update RT buffer pointers
        manager->rt_buffer.tail = (manager->rt_buffer.tail + 1) % RT_BUFFER_SIZE;
        manager->rt_buffer.count--;
        if (manager->rt_buffer.count < RT_BUFFER_SIZE) {
            manager->rt_buffer.is_full = false;
        }

        processed_count++;

        ESP_LOGD(TAG, "Processed packet %zu: esp_timer=%lld, calculated_epoch=%ld", 
                 processed_count, rt_packet->esp_timer_us, backward_epoch);
    }

    ESP_LOGI(TAG, "Processed %zu packets from RT to SO buffer", processed_count);
    return ESP_OK;
}

bool timestamp_so_buffer_ready(timestamp_manager_t *manager)
{
    if (!manager) {
        return false;
    }
    
    return manager->so_buffer.count >= manager->so_buffer_threshold;
}

esp_err_t timestamp_get_so_packets(timestamp_manager_t *manager, packet_t *packets, 
                                   size_t max_packets, size_t *actual_count)
{
    if (!manager || !packets || !actual_count) {
        ESP_LOGE(TAG, "Invalid parameters for getting SO packets");
        return ESP_FAIL;
    }

    *actual_count = 0;
    size_t to_copy = (manager->so_buffer.count < max_packets) ? 
                     manager->so_buffer.count : max_packets;

    for (size_t i = 0; i < to_copy; i++) {
        size_t index = (manager->so_buffer.tail + i) % SO_BUFFER_SIZE;
        memcpy(&packets[i], &manager->so_buffer.packets[index], sizeof(packet_t));
        (*actual_count)++;
    }

    ESP_LOGD(TAG, "Retrieved %zu packets from SO buffer", *actual_count);
    return ESP_OK;
}

esp_err_t timestamp_remove_so_packets(timestamp_manager_t *manager, size_t count)
{
    if (!manager || count == 0) {
        ESP_LOGE(TAG, "Invalid parameters for removing SO packets");
        return ESP_FAIL;
    }

    if (count > manager->so_buffer.count) {
        ESP_LOGW(TAG, "Trying to remove more packets than available");
        count = manager->so_buffer.count;
    }

    manager->so_buffer.tail = (manager->so_buffer.tail + count) % SO_BUFFER_SIZE;
    manager->so_buffer.count -= count;
    if (manager->so_buffer.count < SO_BUFFER_SIZE) {
        manager->so_buffer.is_full = false;
    }

    ESP_LOGD(TAG, "Removed %zu packets from SO buffer", count);
    return ESP_OK;
}

void timestamp_get_stats(timestamp_manager_t *manager, size_t *rt_count, size_t *so_count)
{
    if (!manager) {
        if (rt_count) *rt_count = 0;
        if (so_count) *so_count = 0;
        return;
    }

    if (rt_count) *rt_count = manager->rt_buffer.count;
    if (so_count) *so_count = manager->so_buffer.count;
}