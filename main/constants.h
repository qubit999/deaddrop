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
#define BLE_DISCONNECT_GRACE_PERIOD_SEC 15

// Storage limits
#define MAX_NOTE_SIZE_BYTES 4096
#define MAX_NOTE_COUNT 0  // 0 = unlimited, fills naturally
#define MAX_TITLE_LENGTH 128
#define SPIFFS_BASE_PATH "/spiffs"
#define SPIFFS_PARTITION_LABEL "storage"
#define SPIFFS_MAX_FILES 10

// Error handling
#define ERROR_LED_GPIO 2  // Built-in LED on most ESP32-S3 boards

// Logging
#define LOG_LEVEL_DEFAULT ESP_LOG_INFO

#endif // CONSTANTS_H
