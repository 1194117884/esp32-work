#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "audio-loop";

/* ---- 麦克风 (INMP441) ---- */
#define MIC_BCLK_IO     GPIO_NUM_5
#define MIC_WS_IO       GPIO_NUM_4
#define MIC_DIN_IO      GPIO_NUM_8

/* ---- 功放 (MAX98357A) ---- */
#define AMP_BCLK_IO     GPIO_NUM_16
#define AMP_WS_IO       GPIO_NUM_15
#define AMP_DOUT_IO     GPIO_NUM_17

/* ---- LED ---- */
#define LED_AMP_IO      GPIO_NUM_11
#define LED_MIC_IO      GPIO_NUM_12

/* ---- 按键 ---- */
#define BTN_MUTE_IO     GPIO_NUM_13

/* ---- 音频参数 ---- */
#define SAMPLE_RATE     44100
#define SAMPLE_BITS     I2S_DATA_BIT_WIDTH_16BIT
#define BUF_SAMPLES     512

static bool button_pressed(void)
{
    static uint32_t press_tick = 0;
    static bool was_pressed = false;
    int state = gpio_get_level(BTN_MUTE_IO);
    bool is_pressed = (state == 0);

    if (is_pressed && !was_pressed) {
        /* 下降沿: 记录按下时刻 */
        press_tick = xTaskGetTickCount();
        was_pressed = true;
    } else if (!is_pressed && was_pressed) {
        /* 上升沿: 松开 */
        was_pressed = false;
    }

    /* 按下且稳定 50ms, 且尚未触发过 */
    if (is_pressed && was_pressed &&
        (xTaskGetTickCount() - press_tick) == pdMS_TO_TICKS(50)) {
        return true;
    }
    return false;
}

void app_main(void)
{
    ESP_LOGI(TAG, "音频回环启动");

    /* ====== LED ====== */
    gpio_set_direction(LED_AMP_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_MIC_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AMP_IO, 0);
    gpio_set_level(LED_MIC_IO, 1);

    /* ====== 按键 ====== */
    gpio_set_direction(BTN_MUTE_IO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_MUTE_IO, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "LED: AMP=GPIO%d MIC=GPIO%d, BTN=GPIO%d", LED_AMP_IO, LED_MIC_IO, BTN_MUTE_IO);

    /* ====== 麦克风 I2S (RX) -- 来自已验证的 mic-test ====== */
    i2s_chan_config_t rx_chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = BUF_SAMPLES,
        .auto_clear = true,
    };
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SAMPLE_BITS, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_BCLK_IO,
            .ws = MIC_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = MIC_DIN_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    i2s_chan_handle_t rx_chan;
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    /* ====== 功放 I2S (TX) -- 来自已验证的 mic-test ====== */
    i2s_chan_config_t tx_chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = BUF_SAMPLES,
        .auto_clear = true,
    };
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SAMPLE_BITS, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AMP_BCLK_IO,
            .ws = AMP_WS_IO,
            .dout = AMP_DOUT_IO,
            .din = I2S_GPIO_UNUSED,
        },
    };
    i2s_chan_handle_t tx_chan;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    ESP_LOGI(TAG, "I2S 就绪, 按 GPIO%d 切换静音", BTN_MUTE_IO);

    /* ====== 回环主循环 ====== */
    int16_t buf[BUF_SAMPLES * 2];
    size_t bytes_read, bytes_written;
    bool mic_muted = false;
    int amp_led_timer = 0;

    while (1) {
        /* 按键检测 */
        if (button_pressed()) {
            mic_muted = !mic_muted;
            ESP_LOGI(TAG, "麦克风: %s", mic_muted ? "静音" : "开启");
        }

        /* 麦克风 LED */
        gpio_set_level(LED_MIC_IO, mic_muted ? 0 : 1);

        /* 读取麦克风 */
        esp_err_t ret = i2s_channel_read(rx_chan, buf, sizeof(buf), &bytes_read, pdMS_TO_TICKS(200));

        if (ret == ESP_OK && bytes_read > 0 && !mic_muted) {
            /* 信号强度 */
            int32_t sum = 0;
            int count = bytes_read / sizeof(int16_t);
            for (int i = 0; i < count; i++) sum += abs(buf[i]);

            /* 播放 */
            i2s_channel_write(tx_chan, buf, bytes_read, &bytes_written, pdMS_TO_TICKS(100));

            /* 功放 LED: 有声音亮, 无声音渐灭 */
            if ((sum / count) > 3000) {
                gpio_set_level(LED_AMP_IO, 1);
                amp_led_timer = 50;  /* 保持亮 50ms */
            }
            if (amp_led_timer > 0) {
                amp_led_timer--;
            } else {
                gpio_set_level(LED_AMP_IO, 0);
            }
        } else if (mic_muted) {
            gpio_set_level(LED_AMP_IO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}