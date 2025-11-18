#include "error.h"
#include "constants.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "error";

void error_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ERROR_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(ERROR_LED_GPIO, 0);
}

void error_halt(const char *message)
{
    ESP_LOGE(TAG, "FATAL ERROR: %s", message);
    ESP_LOGE(TAG, "System halted. Please reset the device.");

    // Blink LED in error pattern (fast blink)
    while (1) {
        gpio_set_level(ERROR_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(ERROR_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
