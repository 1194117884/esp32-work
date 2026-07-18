idf_component_register(SRCS "main.c" "ssd1306.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver esp_lcd)