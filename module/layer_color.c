/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include "layer_color.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_WS2812_STRIP)

struct charybdis_led_hsb {
    uint16_t h;
    uint8_t s;
    uint8_t b;
};

static const struct charybdis_led_hsb base_color = {.h = 105, .s = 100, .b = 75};
static const struct charybdis_led_hsb lower_color = {.h = 36, .s = 100, .b = 85};
static const struct charybdis_led_hsb raise_color = {.h = 210, .s = 100, .b = 85};
static const struct charybdis_led_hsb mouse_color = {.h = 0, .s = 100, .b = 85};
static const struct charybdis_led_hsb button_color = {.h = 0, .s = 0, .b = 100};
static const struct charybdis_led_hsb i3_color = {.h = 128, .s = 100, .b = 85};
static const struct charybdis_led_hsb sym_color = {.h = 300, .s = 90, .b = 80};
static const struct charybdis_led_hsb num_color = {.h = 52, .s = 95, .b = 85};
static const struct charybdis_led_hsb fun_color = {.h = 270, .s = 90, .b = 85};
static const struct charybdis_led_hsb off_color = {.h = 0, .s = 0, .b = 0};

#define CHARYBDIS_LED_COUNT 18

#if DT_NODE_EXISTS(DT_NODELABEL(led_strip))
static const struct device *const led_strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));
#endif

static const struct charybdis_led_hsb layer_leds[][CHARYBDIS_LED_COUNT] = {
    [CHARYBDIS_LED_LAYER_BASE] = {
        base_color, base_color, base_color, base_color, base_color, base_color,
        base_color, base_color, base_color, base_color, base_color, base_color,
        base_color, base_color, base_color, base_color, base_color, base_color,
    },
    [CHARYBDIS_LED_LAYER_LOWER] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        lower_color, lower_color, lower_color, lower_color, lower_color, lower_color,
        lower_color, lower_color, lower_color, lower_color, lower_color, lower_color,
    },
    [CHARYBDIS_LED_LAYER_RAISE] = {
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
    },
    [CHARYBDIS_LED_LAYER_MOUSE] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
    },
    [CHARYBDIS_LED_LAYER_SYM] = {
        sym_color, sym_color, sym_color, off_color, off_color, off_color,
        sym_color, sym_color, sym_color, sym_color, sym_color, sym_color,
        off_color, off_color, off_color, sym_color, sym_color, sym_color,
    },
    [CHARYBDIS_LED_LAYER_NUM] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        off_color, off_color, off_color, num_color, num_color, num_color,
        num_color, num_color, num_color, num_color, num_color, num_color,
    },
    [CHARYBDIS_LED_LAYER_FUN] = {
        fun_color, fun_color, fun_color, off_color, off_color, off_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
        off_color, off_color, off_color, fun_color, fun_color, fun_color,
    },
    [CHARYBDIS_LED_LAYER_BUTTON] = {
        button_color, button_color, button_color, button_color, button_color, button_color,
        button_color, button_color, button_color, button_color, button_color, button_color,
        button_color, button_color, button_color, button_color, button_color, button_color,
    },
    [CHARYBDIS_LED_LAYER_I3] = {
        off_color, off_color, off_color, i3_color, i3_color, i3_color,
        i3_color, i3_color, i3_color, i3_color, i3_color, i3_color,
        i3_color, i3_color, i3_color, off_color, off_color, off_color,
    },
};

static struct led_rgb hsb_to_rgb(struct charybdis_led_hsb color) {
    uint16_t hue = color.h % 360U;
    uint8_t sat = (uint8_t)(((uint16_t)color.s * 255U) / 100U);
    uint8_t val = (uint8_t)(((uint16_t)color.b * 255U) / 100U);

    if (sat == 0U) {
        return (struct led_rgb){.r = val, .g = val, .b = val};
    }

    uint16_t region = hue / 60U;
    uint16_t remainder = ((hue % 60U) * 255U) / 60U;
    uint16_t p = (val * (255U - sat)) / 255U;
    uint16_t q = (val * (255U - ((sat * remainder) / 255U))) / 255U;
    uint16_t t = (val * (255U - ((sat * (255U - remainder)) / 255U))) / 255U;

    switch (region % 6U) {
    case 0:
        return (struct led_rgb){.r = val, .g = t, .b = p};
    case 1:
        return (struct led_rgb){.r = q, .g = val, .b = p};
    case 2:
        return (struct led_rgb){.r = p, .g = val, .b = t};
    case 3:
        return (struct led_rgb){.r = p, .g = q, .b = val};
    case 4:
        return (struct led_rgb){.r = t, .g = p, .b = val};
    default:
        return (struct led_rgb){.r = val, .g = p, .b = q};
    }
}

static const struct charybdis_led_hsb *layer_led_map(uint8_t layer) {
    if (layer < (sizeof(layer_leds) / sizeof(layer_leds[0]))) {
        return layer_leds[layer];
    }

    return layer_leds[CHARYBDIS_LED_LAYER_BASE];
}

static int update_leds(uint8_t layer) {
#if DT_NODE_EXISTS(DT_NODELABEL(led_strip))
    if (!device_is_ready(led_strip)) {
        return -ENODEV;
    }

    struct led_rgb pixels[CHARYBDIS_LED_COUNT];
    const struct charybdis_led_hsb *map = layer_led_map(layer);

    for (int i = 0; i < CHARYBDIS_LED_COUNT; i++) {
        pixels[i] = hsb_to_rgb(map[i]);
    }

    return led_strip_update_rgb(led_strip, pixels, CHARYBDIS_LED_COUNT);
#else
    ARG_UNUSED(layer);
    return -ENOTSUP;
#endif
}

int charybdis_layer_color_apply(uint8_t layer) {
    int err = update_leds(layer);

    if (err < 0) {
        LOG_WRN("layer color update failed: %d", err);
    }

    return err;
}

#else

int charybdis_layer_color_apply(uint8_t layer) {
    ARG_UNUSED(layer);
    return -ENOTSUP;
}

#endif
