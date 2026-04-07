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

struct charybdis_led_hsb_adjustment {
    int16_t hue_offset;
    int8_t sat_offset;
    int8_t brightness_offset;
};

enum charybdis_side {
    CHARYBDIS_SIDE_LEFT = 0,
    CHARYBDIS_SIDE_RIGHT = 1,
    CHARYBDIS_SIDE_COUNT = 2,
};

#define CHARYBDIS_LAYER_COUNT (CHARYBDIS_LAYER_SNI + 1)

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
static const struct charybdis_led_hsb keypad_blue_color = {.h = 198, .s = 82, .b = 88};
static const struct charybdis_led_hsb keypad_red_color = {.h = 6, .s = 88, .b = 92};
static const struct charybdis_led_hsb base_led_0_color = {.h = 12, .s = 91, .b = 96};
static const struct charybdis_led_hsb base_led_1_color = {.h = 35, .s = 88, .b = 94};
static const struct charybdis_led_hsb base_led_2_color = {.h = 58, .s = 84, .b = 93};
static const struct charybdis_led_hsb base_led_3_color = {.h = 82, .s = 79, .b = 91};
static const struct charybdis_led_hsb base_led_4_color = {.h = 104, .s = 86, .b = 89};
static const struct charybdis_led_hsb base_led_5_color = {.h = 126, .s = 83, .b = 92};
static const struct charybdis_led_hsb base_led_6_color = {.h = 148, .s = 76, .b = 88};
static const struct charybdis_led_hsb base_led_7_color = {.h = 171, .s = 81, .b = 90};
static const struct charybdis_led_hsb base_led_8_color = {.h = 194, .s = 87, .b = 95};
static const struct charybdis_led_hsb base_led_9_color = {.h = 214, .s = 80, .b = 92};
static const struct charybdis_led_hsb base_led_10_color = {.h = 236, .s = 85, .b = 94};
static const struct charybdis_led_hsb base_led_11_color = {.h = 258, .s = 78, .b = 90};
static const struct charybdis_led_hsb base_led_12_color = {.h = 279, .s = 82, .b = 93};
static const struct charybdis_led_hsb base_led_13_color = {.h = 301, .s = 77, .b = 89};
static const struct charybdis_led_hsb base_led_14_color = {.h = 323, .s = 84, .b = 92};
static const struct charybdis_led_hsb base_led_15_color = {.h = 342, .s = 79, .b = 95};
static const struct charybdis_led_hsb base_led_16_color = {.h = 18, .s = 74, .b = 87};
static const struct charybdis_led_hsb base_led_17_color = {.h = 201, .s = 72, .b = 86};

#define CHARYBDIS_LED_COUNT 18
#define CHARYBDIS_HUE_STEP 12
#define CHARYBDIS_PERCENT_STEP 5

#if DT_NODE_EXISTS(DT_NODELABEL(led_strip))
static const struct device *const led_strip = DEVICE_DT_GET(DT_NODELABEL(led_strip));
#endif

static struct charybdis_led_hsb_adjustment side_layer_adjustments[CHARYBDIS_SIDE_COUNT][CHARYBDIS_LAYER_COUNT];
static uint8_t current_layer = CHARYBDIS_LAYER_BASE;

/*
 * Logical LED order is authored in keymap-like reading order:
 *   0  1  2  3  4
 *   5  6  7  8  9
 *  10 11 12 13 14
 *  15 16 17
 *
 * Physical WS2812 chain order can differ. Update this table if the observed
 * hardware order differs from the logical pattern.
 */
static const uint8_t left_logical_to_physical_led[CHARYBDIS_LED_COUNT] = {
    2, 3, 8,  9, 12,
    1, 4, 7, 10, 13,
    0, 5, 6, 11, 14,
             15, 16, 17,
};

static const uint8_t right_logical_to_physical_led[CHARYBDIS_LED_COUNT] = {
        12,  9, 8, 3, 2,
        13, 10, 7, 4, 1,
        14, 11, 6, 5, 0,
    17, 16, 15,
};

static const struct charybdis_led_hsb left_layer_leds[][CHARYBDIS_LED_COUNT] = {
    [CHARYBDIS_LAYER_BASE] = {
        base_led_0_color, base_led_1_color, base_led_2_color, base_led_3_color, base_led_4_color, base_led_5_color,
        base_led_6_color, base_led_7_color, base_led_8_color, base_led_9_color, base_led_10_color, base_led_11_color,
        base_led_12_color, base_led_13_color, base_led_14_color, base_led_15_color, base_led_16_color, base_led_17_color,
    },
    [CHARYBDIS_LAYER_NUFU] = {
        off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, keypad_blue_color,
        off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, keypad_blue_color,
        off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, keypad_blue_color,
                                         off_color, off_color, keypad_blue_color,
    },
    [CHARYBDIS_LAYER_SYM] = {
        sym_color, sym_color, sym_color, off_color, off_color, off_color,
        sym_color, sym_color, sym_color, sym_color, sym_color, sym_color,
        off_color, off_color, off_color, sym_color, sym_color, sym_color,
    },
    [CHARYBDIS_LAYER_NAV] = {
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
    },
    [CHARYBDIS_LAYER_MED] = {
        fun_color, fun_color, fun_color, off_color, off_color, off_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
        off_color, off_color, off_color, fun_color, fun_color, fun_color,
    },
    [CHARYBDIS_LAYER_POI] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
    },
    [CHARYBDIS_LAYER_SCR] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
    },
    [CHARYBDIS_LAYER_SNI] = {
        base_led_0_color, base_led_1_color, base_led_2_color, base_led_3_color, base_led_4_color, base_led_5_color,
        base_led_6_color, base_led_7_color, base_led_8_color, base_led_9_color, base_led_10_color, base_led_11_color,
        base_led_12_color, base_led_13_color, base_led_14_color, base_led_15_color, base_led_16_color, base_led_17_color,
    },
};

static const struct charybdis_led_hsb right_layer_leds[][CHARYBDIS_LED_COUNT] = {
    [CHARYBDIS_LAYER_BASE] = {
        base_led_0_color, base_led_1_color, base_led_2_color, base_led_3_color, base_led_4_color, base_led_5_color,
        base_led_6_color, base_led_7_color, base_led_8_color, base_led_9_color, base_led_10_color, base_led_11_color,
        base_led_12_color, base_led_13_color, base_led_14_color, base_led_15_color, base_led_16_color, base_led_17_color,
    },
    [CHARYBDIS_LAYER_NUFU] = {
                  off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, keypad_red_color,
                  off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, keypad_red_color,
                  off_color, keypad_blue_color, keypad_blue_color, keypad_blue_color, off_color,
       off_color, keypad_blue_color, keypad_blue_color,
    },
    [CHARYBDIS_LAYER_SYM] = {
        sym_color, sym_color, sym_color, off_color, off_color, off_color,
        sym_color, sym_color, sym_color, sym_color, sym_color, sym_color,
        off_color, off_color, off_color, sym_color, sym_color, sym_color,
    },
    [CHARYBDIS_LAYER_NAV] = {
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        raise_color, raise_color, raise_color, raise_color, raise_color, raise_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
    },
    [CHARYBDIS_LAYER_MED] = {
        fun_color, fun_color, fun_color, off_color, off_color, off_color,
        off_color, off_color, off_color, off_color, off_color, off_color,
        off_color, off_color, off_color, fun_color, fun_color, fun_color,
    },
    [CHARYBDIS_LAYER_POI] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
    },
    [CHARYBDIS_LAYER_SCR] = {
        off_color, off_color, off_color, off_color, off_color, off_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
        mouse_color, mouse_color, mouse_color, mouse_color, mouse_color, mouse_color,
    },
    [CHARYBDIS_LAYER_SNI] = {
        base_led_0_color, base_led_1_color, base_led_2_color, base_led_3_color, base_led_4_color, base_led_5_color,
        base_led_6_color, base_led_7_color, base_led_8_color, base_led_9_color, base_led_10_color, base_led_11_color,
        base_led_12_color, base_led_13_color, base_led_14_color, base_led_15_color, base_led_16_color, base_led_17_color,
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

static struct led_rgb precomputed_pixels[CHARYBDIS_LAYER_COUNT][CHARYBDIS_LED_COUNT];
static bool pixels_ready;

static uint8_t clamp_percent(int value) {
    if (value < 0) {
        return 0;
    }

    if (value > 100) {
        return 100;
    }

    return (uint8_t)value;
}

static enum charybdis_side active_side(void) {
#if defined(CONFIG_SHIELD_CHARYBDIS_LEFT)
    return CHARYBDIS_SIDE_LEFT;
#elif defined(CONFIG_SHIELD_CHARYBDIS_RIGHT)
    return CHARYBDIS_SIDE_RIGHT;
#else
    return CHARYBDIS_SIDE_RIGHT;
#endif
}

static const struct charybdis_led_hsb (*active_layer_leds(void))[CHARYBDIS_LED_COUNT] {
    return active_side() == CHARYBDIS_SIDE_LEFT ? left_layer_leds : right_layer_leds;
}

static const uint8_t *active_logical_to_physical_led(void) {
    return active_side() == CHARYBDIS_SIDE_LEFT ? left_logical_to_physical_led
                                                : right_logical_to_physical_led;
}

static struct charybdis_led_hsb adjusted_layer_color(uint8_t layer, struct charybdis_led_hsb color) {
    struct charybdis_led_hsb_adjustment *adjustment = &side_layer_adjustments[active_side()][layer];
    int16_t hue = (int16_t)color.h + adjustment->hue_offset;

    while (hue < 0) {
        hue += 360;
    }

    color.h = (uint16_t)(hue % 360);
    color.s = clamp_percent((int)color.s + adjustment->sat_offset);
    color.b = clamp_percent((int)color.b + adjustment->brightness_offset);
    return color;
}

static void precompute_layer_pixels(uint8_t layer) {
    const struct charybdis_led_hsb(*layer_leds)[CHARYBDIS_LED_COUNT] = active_layer_leds();
    const uint8_t *logical_to_physical_led = active_logical_to_physical_led();

    for (int logical = 0; logical < CHARYBDIS_LED_COUNT; logical++) {
        uint8_t physical = logical_to_physical_led[logical];
        struct charybdis_led_hsb color = adjusted_layer_color(layer, layer_leds[layer][logical]);

        precomputed_pixels[layer][physical] = hsb_to_rgb(color);
    }
}

static void precompute_all_pixels(void) {
    for (int layer = 0; layer < CHARYBDIS_LAYER_COUNT; layer++) {
        precompute_layer_pixels((uint8_t)layer);
    }

    pixels_ready = true;
}

static int update_leds(uint8_t layer) {
#if DT_NODE_EXISTS(DT_NODELABEL(led_strip))
    if (!device_is_ready(led_strip)) {
        return -ENODEV;
    }

    if (!pixels_ready) {
        precompute_all_pixels();
    }

    uint8_t idx = (layer < CHARYBDIS_LAYER_COUNT) ? layer : CHARYBDIS_LAYER_BASE;
    current_layer = idx;
    return led_strip_update_rgb(led_strip, precomputed_pixels[idx], CHARYBDIS_LED_COUNT);
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

int charybdis_layer_color_adjust(uint8_t control) {
    struct charybdis_led_hsb_adjustment *adjustment = &side_layer_adjustments[active_side()][current_layer];

    switch (control) {
    case CHARYBDIS_LED_ADJUST_HUE_UP:
        adjustment->hue_offset += CHARYBDIS_HUE_STEP;
        break;
    case CHARYBDIS_LED_ADJUST_HUE_DOWN:
        adjustment->hue_offset -= CHARYBDIS_HUE_STEP;
        break;
    case CHARYBDIS_LED_ADJUST_SAT_UP:
        adjustment->sat_offset += CHARYBDIS_PERCENT_STEP;
        break;
    case CHARYBDIS_LED_ADJUST_SAT_DOWN:
        adjustment->sat_offset -= CHARYBDIS_PERCENT_STEP;
        break;
    case CHARYBDIS_LED_ADJUST_BRIGHTNESS_UP:
        adjustment->brightness_offset += CHARYBDIS_PERCENT_STEP;
        break;
    case CHARYBDIS_LED_ADJUST_BRIGHTNESS_DOWN:
        adjustment->brightness_offset -= CHARYBDIS_PERCENT_STEP;
        break;
    default:
        return -EINVAL;
    }

    if (pixels_ready) {
        precompute_layer_pixels(current_layer);
    }

    return charybdis_layer_color_apply(current_layer);
}

#else

int charybdis_layer_color_apply(uint8_t layer) {
    ARG_UNUSED(layer);
    return -ENOTSUP;
}

int charybdis_layer_color_adjust(uint8_t control) {
    ARG_UNUSED(control);
    return -ENOTSUP;
}

#endif
