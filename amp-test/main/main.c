#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "amp-test";

/* I2S 引脚 */
#define I2S_BCLK_IO     GPIO_NUM_16
#define I2S_LRC_IO      GPIO_NUM_15
#define I2S_DIN_IO      GPIO_NUM_17

/* 音频参数 */
#define SAMPLE_RATE     44100
#define SAMPLE_BITS     I2S_DATA_BIT_WIDTH_16BIT
#define BUFFER_SAMPLES  1024

/* 生成正弦波 */
static void generate_sine(int16_t *buf, int samples, float freq, float amplitude)
{
    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * i / SAMPLE_RATE));
    }
}

/* 播放指定频率的提示音 */
static void play_tone(i2s_chan_handle_t tx_chan, float freq, float duration_ms, float volume)
{
    int total_samples = (int)(SAMPLE_RATE * duration_ms / 1000.0f);
    int16_t buf[BUFFER_SAMPLES];
    size_t bytes_written;

    for (int offset = 0; offset < total_samples; offset += BUFFER_SAMPLES) {
        int chunk = BUFFER_SAMPLES;
        if (offset + chunk > total_samples) chunk = total_samples - offset;

        generate_sine(buf, chunk, freq, volume * 32767.0f);

        /* 淡入淡出避免爆破音 */
        if (offset < BUFFER_SAMPLES) {
            for (int i = 0; i < 50 && i < chunk; i++) {
                buf[i] = (int16_t)(buf[i] * i / 50.0f);
            }
        }
        if (offset + chunk > total_samples - BUFFER_SAMPLES) {
            int fade_start = chunk - 50;
            if (fade_start < 0) fade_start = 0;
            for (int i = fade_start; i < chunk; i++) {
                buf[i] = (int16_t)(buf[i] * (chunk - i) / 50.0f);
            }
        }

        i2s_channel_write(tx_chan, buf, chunk * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "功放测试开始");

    /* 配置 I2S 标准模式 */
    i2s_chan_config_t chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 8,
    .dma_frame_num = BUFFER_SAMPLES,
    .auto_clear = true,
};
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SAMPLE_BITS, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_LRC_IO,
            .dout = I2S_DIN_IO,
            .din = I2S_GPIO_UNUSED,
        },
    };

    i2s_chan_handle_t tx_chan;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    ESP_LOGI(TAG, "I2S 初始化完成 (BCLK=%d, LRC=%d, DIN=%d)", I2S_BCLK_IO, I2S_LRC_IO, I2S_DIN_IO);

    /* 简谱音符频率映射 (1=C4)
     * 1=262 2=294 3=330 5=392 6=440 */
    #define N1  262
    #define N2  294
    #define N3  330
    #define N5  392
    #define N6  440
    #define N1H 523  /* 高八度 */

    /* 节拍 (100 BPM) */
    #define Q   500   /* 四分音符 */
    #define H  1000   /* 二分音符 */
    #define DH 1500   /* 附点二分 (3拍) */
    #define GAP  30   /* 音符间隔 */

    typedef struct { float freq; int dur; } note_t;

    /* 东方红 (简谱)
     * |5 5 6 2 -|1 1 6 2 -|5 5 6 1 6 5|1 1 6 2 -|
     * |5 5 6 1 6 5|3 2 1 2 -|2 2 3 5 6 5|3 2 1 6 2 -|
     * |5 5 6 1 6 5|3 5 2 1 6 5 -|6 5 3 2 1 -| */
    const note_t dongfanghong[] = {
        /* 第1-4小节 */
        {N5,Q},{N5,Q},{N6,Q},{N2,H},{GAP,GAP},
        {N1,Q},{N1,Q},{N6,Q},{N2,H},{GAP,GAP},
        {N5,Q},{N5,Q},{N6,Q},{N1,Q},{N6,Q},{N5,Q},
        {N1,Q},{N1,Q},{N6,Q},{N2,H},{GAP,GAP},

        /* 第5-8小节 */
        {N5,Q},{N5,Q},{N6,Q},{N1,Q},{N6,Q},{N5,Q},
        {N3,Q},{N2,Q},{N1,Q},{N2,H},{GAP,GAP},
        {N2,Q},{N2,Q},{N3,Q},{N5,Q},{N6,Q},{N5,Q},
        {N3,Q},{N2,Q},{N1,Q},{N6,Q},{N2,H},{GAP,GAP},

        /* 第9-12小节 */
        {N5,Q},{N5,Q},{N6,Q},{N1,Q},{N6,Q},{N5,Q},
        {N3,Q},{N5,Q},{N2,Q},{N1,Q},{N6,Q},{N5,DH},
        {N6,Q},{N5,Q},{N3,Q},{N2,Q},{N1,DH},
        {GAP,1000},  /* 段落间停顿 */
    };
    int melody_len = sizeof(dongfanghong) / sizeof(note_t);

    /* 循环播放 */
    ESP_LOGI(TAG, "东方红 - 循环播放");
    while (1) {
        for (int i = 0; i < melody_len; i++) {
            if (dongfanghong[i].freq > 10) {
                play_tone(tx_chan, dongfanghong[i].freq, dongfanghong[i].dur, 0.5f);
            }
            vTaskDelay(pdMS_TO_TICKS(dongfanghong[i].dur + GAP));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}