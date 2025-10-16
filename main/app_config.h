#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// WiFi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define WIFI_MAXIMUM_RETRY 5

// SNTP Configuration
#define SNTP_SERVER_1 "pool.ntp.org"
#define SNTP_SERVER_2 "time.nist.gov"
#define SNTP_SERVER_3 "time.google.com"
#define SNTP_SYNC_TIMEOUT_MS 30000

// HTTP POST Configuration
#define HTTP_POST_URL "https://your-server.com/api/data"
#define HTTP_CONTENT_TYPE "application/json"
#define HTTP_AUTH_HEADER "Bearer YOUR_API_TOKEN"
#define USE_HTTPS true

// Buffer Configuration
#define SO_BUFFER_THRESHOLD 10  // Send data when SO buffer has this many packets
#define PACKET_SIMULATION_INTERVAL_MS 1000  // For testing - generate packet every second

// Timezone Configuration
#define TIMEZONE "UTC-0"  // Change to your timezone (e.g., "EST5EDT", "PST8PDT", "CET-1CEST")

#endif // APP_CONFIG_H