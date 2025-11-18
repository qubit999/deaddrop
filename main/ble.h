#ifndef BLE_H
#define BLE_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize BLE stack and start advertising
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_init(void);

/**
 * @brief Stop BLE advertising
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_stop(void);

/**
 * @brief Start BLE advertising
 * 
 * @return ESP_OK on success
 */
esp_err_t ble_start(void);

/**
 * @brief Check if BLE is currently advertising
 * 
 * @return true if advertising, false otherwise
 */
bool ble_is_advertising(void);

/**
 * @brief Set callback to be called when BLE device connects
 * 
 * @param callback Function to call on connection
 */
void ble_set_connect_callback(void (*callback)(void));

/**
 * @brief Set callback to be called when BLE device disconnects
 * 
 * @param callback Function to call on disconnection
 */
void ble_set_disconnect_callback(void (*callback)(void));

#endif // BLE_H
