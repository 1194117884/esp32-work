#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ssd1306.h"

static const char *TAG = "screen-test";

void app_main(void)
{
    ESP_LOGI(TAG, "屏幕测试开始");

    /* 初始化 SSD1306 */
    ssd1306_init();

    /* 清屏 */
    ssd1306_clear();

    /* ---- 第一屏: 基本信息 ---- */
    ssd1306_put_string(0, 0,  "ESP32-S3");
    ssd1306_put_string(0, 10, "OLED 128x32");
    ssd1306_put_string(0, 20, "I2C OK!");
    ssd1306_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, 1);
    ssd1306_refresh();
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ---- 第二屏: 弹跳方块 ---- */
    ssd1306_clear();
    ssd1306_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, 1);

    int x = 10, y = 10;
    int dx = 2, dy = 1;
    int size = 6;

    for (int frame = 0; frame < 300; frame++) {
        ssd1306_fill_rect(x, y, size, size, 0);
        x += dx;
        y += dy;
        if (x <= 1 || x + size >= SSD1306_WIDTH - 1)  dx = -dx;
        if (y <= 1 || y + size >= SSD1306_HEIGHT - 1) dy = -dy;
        if (x < 1) x = 1;
        if (y < 1) y = 1;
        if (x + size > SSD1306_WIDTH - 1)  x = SSD1306_WIDTH - 1 - size;
        if (y + size > SSD1306_HEIGHT - 1) y = SSD1306_HEIGHT - 1 - size;
        ssd1306_fill_rect(x, y, size, size, 1);
        ssd1306_put_string(40, 12, "HELLO");
        ssd1306_refresh();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    /* ---- 最终: 显示状态 ---- */
    ssd1306_clear();
    ssd1306_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, 1);
    ssd1306_put_string(10, 4,  "Screen OK!");
    ssd1306_put_string(10, 16, "ESP32-S3");
    ssd1306_refresh();

    ESP_LOGI(TAG, "屏幕测试完成!");
}