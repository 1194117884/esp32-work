#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "led-blink";

/* ESP32-S3 常见板载 LED 引脚:
 * - GPIO 48: ESP32-S3-DevKitC 等官方开发板
 * - GPIO 2:  部分第三方开发板
 * 如果你的板子 LED 不亮，修改下面的引脚号 */
#define LED_GPIO    48

void app_main(void)
{
    ESP_LOGI(TAG, "LED Blink 初始化, GPIO=%d", LED_GPIO);

    /* 配置 GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        ESP_LOGI(TAG, "LED ON");
        vTaskDelay(pdMS_TO_TICKS(500));

        gpio_set_level(LED_GPIO, 0);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}