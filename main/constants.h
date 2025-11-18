#ifndef CONSTANTS_H
#define CONSTANTS_H

// Device naming
#define DEVICE_NAME_BLE "DeadDrop"
#define WIFI_AP_SSID "DeadDrop"
#define WIFI_AP_PASSWORD ""  // Empty string = open network, otherwise WPA2

// Network configuration
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_GATEWAY "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"

// BLE configuration
#define BLE_DISCONNECT_GRACE_PERIOD_SEC 10
#define WIFI_TIMEOUT_MINUTES 5
#define WIFI_TIMEOUT_MS (WIFI_TIMEOUT_MINUTES * 60 * 1000)

// Storage limits
#define MAX_NOTE_SIZE_BYTES 4096
#define MAX_NOTE_COUNT 0  // 0 = unlimited, fills naturally
#define MAX_TITLE_LENGTH 128
#define SPIFFS_BASE_PATH "/spiffs"
#define SPIFFS_PARTITION_LABEL "storage"
#define SPIFFS_MAX_FILES 10

// Security configuration
#define RATE_LIMIT_ATTEMPTS 10
#define RATE_LIMIT_WINDOW_MIN 10
#define PBKDF2_ITERATIONS 10000
#define AES_KEY_SIZE 32  // AES-256
#define AES_IV_SIZE 16
#define SALT_SIZE 16
#define SHA256_HASH_SIZE 32

// Error handling
#define ERROR_LED_GPIO 2  // Built-in LED on most ESP32-S3 boards

// Logging
#define LOG_LEVEL_DEFAULT ESP_LOG_INFO

// NVS namespaces
#define NVS_NAMESPACE_STORAGE "storage"
#define NVS_NAMESPACE_RATE_LIMIT "rate_limit"

// HTTP Server
#define HTTP_SERVER_PORT 80
#define HTTP_SERVER_MAX_URI_LEN 512
#define HTTP_SERVER_MAX_RESP_LEN 4096

#endif // CONSTANTS_H
