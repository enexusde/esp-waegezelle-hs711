#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

typedef struct {
    gpio_num_t gpio_dout;
    gpio_num_t gpio_sck;
} hx711_t;

esp_err_t hx711_init(hx711_t *dev, gpio_num_t dout, gpio_num_t sck);

/**
 * Blockiert, bis ein neuer Wert verfügbar ist, und liest einen 24-bit Rohwert.
 * Rückgabe ist ein signextender 32-bit signed Wert.
 */
esp_err_t hx711_read(hx711_t *dev, int32_t *out_value);

/**
 * Setzt den Gain/Channel (optional später):
 * gain = 128, 64, 32
 */
esp_err_t hx711_set_gain(hx711_t *dev, uint8_t gain);
