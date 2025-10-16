# ESP32-C3 SNTP HTTP IoT Project

A sophisticated embedded system project for ESP32-C3 that implements HTTP and SNTP protocols with advanced timestamp management and backward epoch timestamping capabilities.

## Features

- **Advanced Timestamp Management**: Implements backward epoch timestamping using ESP timer
- **Dual Buffer System**: RT buffer for packets before SNTP sync, SO buffer for timestamped packets
- **SNTP Synchronization**: Automatic time synchronization with multiple NTP servers
- **HTTP POST Integration**: Automatic data transmission when threshold is reached
- **WiFi Connectivity**: Robust WiFi connection with retry mechanism
- **JSON Data Format**: Structured data transmission with packet metadata

## Architecture

### Core Components

1. **timestamp.c**: Manages packet buffering and backward timestamping
   - RT Buffer: Stores packets before SNTP synchronization
   - SO Buffer: Stores timestamped packets ready for transmission
   - Backward timestamping: Calculates epoch time for historical packets

2. **post_data.c**: Handles HTTP POST transmission
   - Threshold-based sending
   - JSON packet serialization
   - Automatic retry and error handling

3. **main.c**: Application orchestration
   - WiFi setup and management
   - SNTP initialization and callbacks
   - Task coordination

## How It Works

1. **Before SNTP Sync**: Incoming packets are stored in RT buffer with ESP timer timestamps
2. **SNTP Synchronization**: When NTP sync occurs, all RT buffer packets get backward-calculated epoch timestamps
3. **Buffer Transfer**: Timestamped packets move from RT buffer to SO buffer
4. **Automatic Transmission**: When SO buffer reaches threshold, data is sent via HTTP POST
5. **Continuous Operation**: New packets go directly to SO buffer with real-time timestamps

## Configuration

Edit `main/app_config.h` to configure:

```c
// WiFi Settings
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

// HTTP Endpoint
#define HTTP_POST_URL "https://your-server.com/api/data"
#define HTTP_AUTH_HEADER "Bearer YOUR_API_TOKEN"

// Buffer Settings
#define SO_BUFFER_THRESHOLD 10  // Send when SO buffer has 10 packets

// NTP Servers
#define SNTP_SERVER_1 "pool.ntp.org"
#define SNTP_SERVER_2 "time.nist.gov"
```

## Building and Flashing

### Prerequisites
- ESP-IDF v4.4 or later
- ESP32-C3 development board

### Build Commands
```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure project
idf.py set-target esp32c3
idf.py menuconfig

# Build project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

## Data Format

Packets are transmitted as JSON arrays:

```json
{
  "packet_count": 5,
  "timestamp": 1697123456,
  "packets": [
    {
      "epoch_time": 1697123450,
      "esp_timer_us": 1234567890,
      "has_epoch": true,
      "data": "48656c6c6f20576f726c64",
      "data_len": 11
    }
  ]
}
```

## Buffer Management

- **RT Buffer**: 100 packets (configurable)
- **SO Buffer**: 200 packets (configurable)
- **Threshold**: Configurable trigger for HTTP transmission
- **Memory Efficient**: Circular buffers with overflow protection

## Monitoring and Debugging

The application provides comprehensive logging:

```
I (12345) MAIN: === Status Report ===
I (12346) MAIN: Current time: 1697123456
I (12347) MAIN: SNTP synced: YES
I (12348) MAIN: RT Buffer: 0 packets
I (12349) MAIN: SO Buffer: 8 packets (threshold: 10)
I (12350) MAIN: Free heap: 245760 bytes
```

## Error Handling

- **WiFi Disconnection**: Automatic reconnection with exponential backoff
- **SNTP Failure**: Graceful degradation with retry mechanism
- **HTTP Errors**: Packet retention and retry logic
- **Buffer Overflow**: Oldest packet dropping with logging

## Performance Characteristics

- **Memory Usage**: ~50KB RAM for buffers and HTTP client
- **CPU Usage**: Minimal, event-driven architecture
- **Network Efficiency**: Batched transmission reduces overhead
- **Time Accuracy**: Microsecond precision with ESP timer

## Use Cases

- **IoT Data Logging**: Sensor data with precise timestamps
- **Network Monitoring**: Packet capture with time correlation
- **Industrial Applications**: Event logging with backward timestamping
- **Research Projects**: Time-series data collection

## Troubleshooting

### Common Issues

1. **WiFi Connection Failed**
   - Check SSID and password in `app_config.h`
   - Verify WiFi signal strength

2. **SNTP Sync Timeout**
   - Check internet connectivity
   - Verify NTP server accessibility
   - Adjust `SNTP_SYNC_TIMEOUT_MS`

3. **HTTP POST Failures**
   - Verify server URL and authentication
   - Check SSL certificates for HTTPS
   - Monitor server logs

4. **Buffer Overflow**
   - Increase buffer sizes in header files
   - Reduce packet generation rate
   - Lower SO buffer threshold

### Debug Commands

```bash
# Monitor serial output
idf.py monitor

# Enable verbose logging
idf.py menuconfig
# Component config -> Log output -> Default log verbosity -> Verbose

# Check memory usage
idf.py size
```

## License

This project is provided as-is for educational and development purposes. 
