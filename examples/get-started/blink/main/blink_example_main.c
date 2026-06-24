#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "hx711.h"

static const char *TAG = "example";

#define BLINK_GPIO 8

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void blink_led(void)
{
    if (s_led_state) {
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

void app_main(void)
{
    hx711_t hx;

    // PINS ANPASSEN!
    ESP_ERROR_CHECK(hx711_init(&hx, GPIO_NUM_4, GPIO_NUM_5));

    // optional Gain setzen (Default 128):
    ESP_ERROR_CHECK(hx711_set_gain(&hx, 128));
    
    
    configure_led();
    long speed = CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS;
    long min = 100000;
    long max = -100000;
    while (1) {
    
    	int32_t raw = 0;
        esp_err_t err = hx711_read(&hx, &raw);
        
        if (err == ESP_OK) {
            if (raw > max) {
              max = (long)raw;
            }
            if (raw < min) {
              min = (long)raw;
            }
            speed = (raw / 100000) + 3;
            speed = speed;
            ESP_LOGI(TAG, "HX711 raw: %ld   (min: %ld, max:%ld, speed:%ld)", (long)raw, min, max, speed);
        } else {
            ESP_LOGE(TAG, "HX711 read error: %s", esp_err_to_name(err));
            speed = CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS;
        }
    
    
        //ESP_LOGI(TAG, "Turning the LED %s! GPIO %i", s_led_state == true ? "ON" : "OFF", BLINK_GPIO);
        blink_led();
        s_led_state = !s_led_state;
        vTaskDelay(speed);
    }
}
