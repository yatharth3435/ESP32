#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "timestamp.h"
#include "post_data.h"
#include "app_config.h"

static const char *TAG = "MAIN";

// Event group for WiFi and SNTP synchronization
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define SNTP_SYNCED_BIT    BIT2

// Global managers
static timestamp_manager_t ts_manager;
static post_data_manager_t post_manager;

// WiFi retry counter
static int s_retry_num = 0;

// SNTP sync status
static bool sntp_sync_completed = false;

// Task handles
static TaskHandle_t packet_generator_task_handle = NULL;
static TaskHandle_t data_sender_task_handle = NULL;

// WiFi event handler
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// SNTP time synchronization callback
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP time synchronized: %ld", tv->tv_sec);
    
    if (!sntp_sync_completed) {
        sntp_sync_completed = true;
        
        // Notify timestamp manager about SNTP sync
        esp_err_t ret = timestamp_sntp_synced(&ts_manager, tv->tv_sec);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to notify timestamp manager about SNTP sync");
        }
        
        xEventGroupSetBits(s_wifi_event_group, SNTP_SYNCED_BIT);
    }
}

// Initialize WiFi
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Wait for WiFi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// Initialize SNTP
void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SNTP_SERVER_1);
    esp_sntp_setservername(1, SNTP_SERVER_2);
    esp_sntp_setservername(2, SNTP_SERVER_3);
    
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Set timezone
    setenv("TZ", TIMEZONE, 1);
    tzset();
}

// Packet generator task (simulates incoming packets)
void packet_generator_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Packet generator task started");
    
    uint32_t packet_counter = 0;
    
    while (1) {
        // Generate a test packet
        char test_data[64];
        snprintf(test_data, sizeof(test_data), "Test packet #%lu from ESP32-C3", packet_counter++);
        
        // Add packet to timestamp manager
        esp_err_t ret = timestamp_add_packet(&ts_manager, (uint8_t*)test_data, strlen(test_data));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add packet to timestamp manager");
        } else {
            ESP_LOGD(TAG, "Added packet #%lu", packet_counter - 1);
        }
        
        // Get buffer statistics
        size_t rt_count, so_count;
        timestamp_get_stats(&ts_manager, &rt_count, &so_count);
        ESP_LOGI(TAG, "Buffer stats - RT: %zu, SO: %zu, SNTP synced: %s", 
                 rt_count, so_count, sntp_sync_completed ? "YES" : "NO");
        
        vTaskDelay(pdMS_TO_TICKS(PACKET_SIMULATION_INTERVAL_MS));
    }
}

// Data sender task (checks threshold and sends data)
void data_sender_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Data sender task started");
    
    // Wait for SNTP synchronization before starting
    xEventGroupWaitBits(s_wifi_event_group, SNTP_SYNCED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "SNTP synchronized, data sender task active");
    
    while (1) {
        // Check if we need to send data
        esp_err_t ret = post_data_check_and_send(&post_manager);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to check and send data");
        }
        
        // Check every 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C3 SNTP HTTP IoT Application Starting");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize timestamp manager
    ret = timestamp_init(&ts_manager, SO_BUFFER_THRESHOLD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize timestamp manager");
        return;
    }
    ESP_LOGI(TAG, "Timestamp manager initialized");

    // Initialize WiFi
    wifi_init_sta();

    // Initialize SNTP
    initialize_sntp();

    // Initialize HTTP POST manager
    http_config_t http_config = {
        .timeout_ms = 10000,
        .use_https = USE_HTTPS
    };
    strncpy(http_config.url, HTTP_POST_URL, MAX_URL_LENGTH - 1);
    strncpy(http_config.content_type, HTTP_CONTENT_TYPE, MAX_HEADER_LENGTH - 1);
    strncpy(http_config.auth_header, HTTP_AUTH_HEADER, MAX_HEADER_LENGTH - 1);

    ret = post_data_init(&post_manager, &ts_manager, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize POST data manager");
        return;
    }
    ESP_LOGI(TAG, "POST data manager initialized");

    // Wait for SNTP synchronization
    ESP_LOGI(TAG, "Waiting for SNTP synchronization...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                           SNTP_SYNCED_BIT, 
                                           pdFALSE, 
                                           pdFALSE, 
                                           pdMS_TO_TICKS(SNTP_SYNC_TIMEOUT_MS));
    
    if (bits & SNTP_SYNCED_BIT) {
        ESP_LOGI(TAG, "SNTP synchronization completed");
        
        // Print current time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);
    } else {
        ESP_LOGW(TAG, "SNTP synchronization timeout, continuing anyway");
    }

    // Create tasks
    xTaskCreate(packet_generator_task, "packet_gen", 4096, NULL, 5, &packet_generator_task_handle);
    xTaskCreate(data_sender_task, "data_sender", 8192, NULL, 4, &data_sender_task_handle);

    ESP_LOGI(TAG, "Application initialization completed");
    
    // Main loop - print statistics periodically
    while (1) {
        size_t rt_count, so_count;
        timestamp_get_stats(&ts_manager, &rt_count, &so_count);
        
        time_t now;
        time(&now);
        
        ESP_LOGI(TAG, "=== Status Report ===");
        ESP_LOGI(TAG, "Current time: %ld", now);
        ESP_LOGI(TAG, "SNTP synced: %s", sntp_sync_completed ? "YES" : "NO");
        ESP_LOGI(TAG, "RT Buffer: %zu packets", rt_count);
        ESP_LOGI(TAG, "SO Buffer: %zu packets (threshold: %d)", so_count, SO_BUFFER_THRESHOLD);
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "====================");
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Print stats every 30 seconds
    }
}