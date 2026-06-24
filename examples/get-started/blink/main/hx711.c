#include "hx711.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "HX711";

static uint8_t s_gain_pulses = 1; // 1=128, 3=64, 2=32 (laut Datenblatt)

esp_err_t hx711_init(hx711_t *dev, gpio_num_t dout, gpio_num_t sck)
{
    if (!dev) return ESP_ERR_INVALID_ARG;

    dev->gpio_dout = dout;
    dev->gpio_sck  = sck;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dout),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.pin_bit_mask = (1ULL << sck);
    io_conf.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_set_level(dev->gpio_sck, 0);

    // Optional: ein paar Dummy-Reads zum „Aufwachen“
    int32_t dummy;
    for (int i = 0; i < 3; i++) {
        hx711_read(dev, &dummy);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ESP_OK;
}

esp_err_t hx711_set_gain(hx711_t *dev, uint8_t gain)
{
    // Gain-Auswahl entspricht der Anzahl zusätzlicher CLKs nach den 24 Bits:
    // 1 Puls: Channel A, Gain 128
    // 2 Pulse: Channel B, Gain 32
    // 3 Pulse: Channel A, Gain 64
    switch (gain) {
        case 128:
            s_gain_pulses = 1;
            break;
        case 64:
            s_gain_pulses = 3;
            break;
        case 32:
            s_gain_pulses = 2;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    // Einmal lesen, um neuen Gain zu „übernehmen“
    int32_t dummy;
    return hx711_read(dev, &dummy);
}

esp_err_t hx711_read(hx711_t *dev, int32_t *out_value)
{
    if (!dev || !out_value) return ESP_ERR_INVALID_ARG;

    // 1) Warten, bis HX711 bereit (DOUT = LOW)
    // Timeout z.B. 100 ms
    const TickType_t timeout = pdMS_TO_TICKS(100);
    TickType_t start = xTaskGetTickCount();

    while (gpio_get_level(dev->gpio_dout) == 1) {
        if (xTaskGetTickCount() - start > timeout) {
            ESP_LOGE(TAG, "Timeout waiting for HX711 ready");
            return ESP_ERR_TIMEOUT;
        }
        // kleine Pause, CPU nicht burnen
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // 2) 24 Bits einlesen, MSB-first
    int32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(dev->gpio_sck, 1);
        // kurze Delay (HX711 max. 50kHz Clock -> wir sind eh langsam)
        esp_rom_delay_us(1);
        value = (value << 1) | gpio_get_level(dev->gpio_dout);
        gpio_set_level(dev->gpio_sck, 0);
        esp_rom_delay_us(1);
    }

    // 3) Gain-Pulse (1–3 zusätzliche Clocks)
    for (int i = 0; i < s_gain_pulses; i++) {
        gpio_set_level(dev->gpio_sck, 1);
        esp_rom_delay_us(1);
        gpio_set_level(dev->gpio_sck, 0);
        esp_rom_delay_us(1);
    }

    // 4) 24-bit 2er-Komplement nach 32-bit sign erweitern
    if (value & 0x800000) {
        value |= ~0xFFFFFF;  // sign extend negativ
    }

    *out_value = value;
    return ESP_OK;
}
