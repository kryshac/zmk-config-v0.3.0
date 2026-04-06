#pragma once

#include <stdint.h>

enum charybdis_led_layer {
    CHARYBDIS_LED_LAYER_BASE = 0,
    CHARYBDIS_LED_LAYER_LOWER = 1,
    CHARYBDIS_LED_LAYER_RAISE = 2,
    CHARYBDIS_LED_LAYER_MOUSE = 3,
    CHARYBDIS_LED_LAYER_SYM = 4,
    CHARYBDIS_LED_LAYER_NUM = 5,
    CHARYBDIS_LED_LAYER_FUN = 6,
    CHARYBDIS_LED_LAYER_BUTTON = 7,
    CHARYBDIS_LED_LAYER_I3 = 8,
    CHARYBDIS_LED_LAYER_I3_NUM = 9,
};

int charybdis_layer_color_apply(uint8_t layer);
