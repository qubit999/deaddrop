#include "ble.h"
#include "constants.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble";
static void (*connect_callback)(void) = NULL;
static void (*disconnect_callback)(void) = NULL;

// BLE event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE GAP event: Connect");
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connection established");
            if (connect_callback != NULL) {
                connect_callback();
            }
        } else {
            ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
            // Restart advertising
            ble_start();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE GAP event: Disconnect; reason=%d", event->disconnect.reason);
        if (disconnect_callback != NULL) {
            disconnect_callback();
        }
        // Restart advertising
        ble_start();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE GAP event: Advertise complete");
        break;

    default:
        break;
    }
    return 0;
}

// Start advertising
esp_err_t ble_start(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    // Set advertising parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable

    // Set advertising data
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME_BLE;
    fields.name_len = strlen(DEVICE_NAME_BLE);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising data; rc=%d", rc);
        return ESP_FAIL;
    }

    // Start advertising
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                          &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE advertising started as '%s'", DEVICE_NAME_BLE);
    return ESP_OK;
}

// Stop advertising
esp_err_t ble_stop(void)
{
    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising; rc=%d", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BLE advertising stopped");
    return ESP_OK;
}

// BLE host task
static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// Sync callback
static void ble_on_sync(void)
{
    ESP_LOGI(TAG, "BLE host synced");
    // Start advertising
    ble_start();
}

// Reset callback
static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset; reason=%d", reason);
}

// Initialize BLE
esp_err_t ble_init(void)
{
    esp_err_t ret;

    // Initialize NVS for BLE
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE; error=%d", ret);
        return ret;
    }

    // Initialize BLE host configuration
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Set device name
    ret = ble_svc_gap_device_name_set(DEVICE_NAME_BLE);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", ret);
        return ESP_FAIL;
    }

    // Initialize GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Start BLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE initialized");
    return ESP_OK;
}

bool ble_is_advertising(void)
{
    return ble_gap_adv_active();
}

// Set connection callback
void ble_set_connect_callback(void (*callback)(void))
{
    connect_callback = callback;
}

// Set disconnection callback
void ble_set_disconnect_callback(void (*callback)(void))
{
    disconnect_callback = callback;
}
