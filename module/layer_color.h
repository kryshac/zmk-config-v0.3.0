#pragma once

#include <stdint.h>

enum charybdis_layer {
    CHARYBDIS_LAYER_BASE = 0,
    CHARYBDIS_LAYER_NUFU = 1,
    CHARYBDIS_LAYER_SYM = 2,
    CHARYBDIS_LAYER_NAV = 3,
    CHARYBDIS_LAYER_MED = 4,
    CHARYBDIS_LAYER_POI = 5,
    CHARYBDIS_LAYER_SCR = 6,
    CHARYBDIS_LAYER_SNI = 7,
};

enum charybdis_led_adjustment {
    CHARYBDIS_LED_ADJUST_HUE_UP = 0,
    CHARYBDIS_LED_ADJUST_HUE_DOWN = 1,
    CHARYBDIS_LED_ADJUST_SAT_UP = 2,
    CHARYBDIS_LED_ADJUST_SAT_DOWN = 3,
    CHARYBDIS_LED_ADJUST_BRIGHTNESS_UP = 4,
    CHARYBDIS_LED_ADJUST_BRIGHTNESS_DOWN = 5,
};

int charybdis_layer_color_apply(uint8_t layer);
int charybdis_layer_color_adjust(uint8_t control);
