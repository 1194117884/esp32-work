#pragma once

#include <stdint.h>
#include "driver/i2c.h"

/* ---- 屏幕参数 ---- */
#define SSD1306_I2C_ADDR    0x3C
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      32
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)  /* 4 */

/* ---- I2C 引脚 ---- */
#define I2C_SCL_IO          GPIO_NUM_7
#define I2C_SDA_IO          GPIO_NUM_6
#define I2C_FREQ            400000
#define I2C_PORT            I2C_NUM_0

/* ---- 初始化 ---- */
void ssd1306_init(void);

/* ---- 绘图 ---- */
void ssd1306_clear(void);
void ssd1306_refresh(void);
void ssd1306_set_pixel(int x, int y, int on);
void ssd1306_draw_rect(int x, int y, int w, int h, int on);
void ssd1306_fill_rect(int x, int y, int w, int h, int on);

/* ---- 文字 ---- */
void ssd1306_put_char(int x, int y, char c);
void ssd1306_put_string(int x, int y, const char *str);