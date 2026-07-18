#include "stdio.h"
#include "string.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void app_main() {
    while(1) {
        printf("start join wifi\n");
        char *ssid = "jinruan";
        char *password = "1234522345";
        wifi_config_t wifi_config = {
            .sta = {
                .ssid = ssid,
                .password = password,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        wifi_sta_set_config(&wifi_config);
        wifi_sta_connect();
        printf("end join wifi\n");
        // get ip address
        uint8_t ip[4];
        wifi_get_ip_info(STATION_IF, ip);
        printf("ip address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}