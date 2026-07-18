#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "mic-test";

/* ---- 麦克风 (INMP441) ---- */
#define MIC_BCLK_IO     GPIO_NUM_5
#define MIC_WS_IO       GPIO_NUM_4
#define MIC_DIN_IO      GPIO_NUM_8

/* ---- 功放 (MAX98357A) ---- */
#define AMP_BCLK_IO     GPIO_NUM_16
#define AMP_WS_IO       GPIO_NUM_15
#define AMP_DOUT_IO     GPIO_NUM_17

/* ---- 音频参数 ---- */
#define SAMPLE_RATE     44100
#define SAMPLE_BITS     I2S_DATA_BIT_WIDTH_16BIT
#define BUF_SAMPLES     512

void app_main(void)
{
    ESP_LOGI(TAG, "麦克风回环测试开始");

    /* ====== 麦克风 I2S (RX) ====== */
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
    ESP_LOGI(TAG, "麦克风 I2S 就绪 (BCLK=%d, WS=%d, DIN=%d)", MIC_BCLK_IO, MIC_WS_IO, MIC_DIN_IO);

    /* ====== 功放 I2S (TX) ====== */
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
    ESP_LOGI(TAG, "功放 I2S 就绪 (BCLK=%d, WS=%d, DOUT=%d)", AMP_BCLK_IO, AMP_WS_IO, AMP_DOUT_IO);

    /* ====== 回环: 麦克风 → 功放 ====== */
    int16_t buf[BUF_SAMPLES * 2];  /* 立体声: 2 通道 */
    size_t bytes_read, bytes_written;

    ESP_LOGI(TAG, "开始回环, 对着麦克风说话...");

    while (1) {
        /* 读取麦克风数据 */
        esp_err_t ret = i2s_channel_read(rx_chan, buf, sizeof(buf), &bytes_read, pdMS_TO_TICKS(500));

        if (ret == ESP_OK && bytes_read > 0) {
            /* 直接写入功放播放 */
            i2s_channel_write(tx_chan, buf, bytes_read, &bytes_written, pdMS_TO_TICKS(100));
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "麦克风无数据, 检查接线 (L/R 接 GND 了吗?)");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}