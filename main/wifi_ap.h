#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * Initialize WiFi in AP mode
 */
esp_err_t wifi_ap_init(void);

/**
 * Start WiFi AP
 */
esp_err_t wifi_ap_start(void);

/**
 * Stop WiFi AP
 */
esp_err_t wifi_ap_stop(void);

/**
 * Check if WiFi AP is running
 */
bool wifi_ap_is_running(void);

#endif // WIFI_AP_H
