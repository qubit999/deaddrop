#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "constants.h"
#include "error.h"
#include "storage.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "ble.h"

static const char *TAG = "main";
static TimerHandle_t grace_period_timer = NULL;
static bool wifi_active = false;
static bool ble_connected = false;

#define GRACE_PERIOD_MS (15 * 1000)  // 15 seconds

// Forward declarations
static void on_ble_connect(void);
static void on_ble_disconnect(void);
static void grace_period_callback(TimerHandle_t xTimer);
static void enable_wifi(void);
static void disable_wifi(void);

// BLE connection callback - triggers WiFi AP
static void on_ble_connect(void)
{
    ESP_LOGI(TAG, "BLE connected");
    ble_connected = true;

    // Cancel grace period if it's running
    if (xTimerIsTimerActive(grace_period_timer)) {
        xTimerStop(grace_period_timer, 0);
        ESP_LOGI(TAG, "Grace period cancelled - BLE reconnected");
    }

    // Enable WiFi if not already active
    if (!wifi_active) {
        enable_wifi();
    }
}

// BLE disconnection callback - starts grace period
static void on_ble_disconnect(void)
{
    ESP_LOGI(TAG, "BLE disconnected - starting %d second grace period", GRACE_PERIOD_MS / 1000);
    ble_connected = false;

    // Start grace period timer
    xTimerStart(grace_period_timer, 0);
}

// Grace period timeout - disables WiFi
static void grace_period_callback(TimerHandle_t xTimer)
{
    if (!ble_connected && wifi_active) {
        ESP_LOGI(TAG, "Grace period expired - disabling WiFi");
        disable_wifi();
    }
}

// Enable WiFi AP and web server
static void enable_wifi(void)
{
    if (wifi_active) {
        ESP_LOGI(TAG, "WiFi already active");
        return;
    }

    ESP_LOGI(TAG, "Enabling WiFi AP");

    // Start WiFi AP
    if (wifi_ap_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi AP");
        return;
    }

    // Start web server
    if (web_server_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        wifi_ap_stop();
        return;
    }

    wifi_active = true;
    ESP_LOGI(TAG, "WiFi AP enabled");
    ESP_LOGI(TAG, "Connect to WiFi: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Open browser to: https://%s", WIFI_AP_IP);
}

// Disable WiFi and return to BLE-only mode
static void disable_wifi(void)
{
    if (!wifi_active) {
        return;
    }

    ESP_LOGI(TAG, "Disabling WiFi AP");

    // Stop web server
    web_server_stop();

    // Stop WiFi AP
    wifi_ap_stop();

    wifi_active = false;
    ESP_LOGI(TAG, "Returned to BLE-only mode");
}

void app_main(void)
{
    // Set log level
    esp_log_level_set("*", LOG_LEVEL_DEFAULT);

    ESP_LOGI(TAG, "=== DeadDrop Starting ===");
    ESP_LOGI(TAG, "Device: %s", DEVICE_NAME_BLE);
    ESP_LOGI(TAG, "WiFi AP: %s", WIFI_AP_SSID);

    // Initialize error handler
    error_init();
    ESP_LOGI(TAG, "Error handler initialized");

    // Initialize storage (NVS + SPIFFS)
    if (storage_init() != ESP_OK) {
        error_halt("Failed to initialize storage");
    }
    ESP_LOGI(TAG, "Storage initialized");

    // Initialize WiFi AP (but don't start yet)
    if (wifi_ap_init() != ESP_OK) {
        error_halt("Failed to initialize WiFi AP");
    }
    ESP_LOGI(TAG, "WiFi AP initialized");

    // Create grace period timer
    grace_period_timer = xTimerCreate("grace_period",
                                      pdMS_TO_TICKS(GRACE_PERIOD_MS),
                                      pdFALSE,  // One-shot timer
                                      NULL,
                                      grace_period_callback);
    if (grace_period_timer == NULL) {
        error_halt("Failed to create grace period timer");
    }

    // Initialize BLE
    if (ble_init() != ESP_OK) {
        error_halt("Failed to initialize BLE");
    }
    ESP_LOGI(TAG, "BLE initialized");

    // Set BLE callbacks
    ble_set_connect_callback(on_ble_connect);
    ble_set_disconnect_callback(on_ble_disconnect);

    ESP_LOGI(TAG, "=== DeadDrop Ready (BLE-only mode) ===");
    ESP_LOGI(TAG, "Connect via BLE to '%s' to enable WiFi AP", DEVICE_NAME_BLE);

    // Main loop - keep system running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
