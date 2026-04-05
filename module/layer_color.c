/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW) &&                                                     \
    (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

enum charybdis_layer_id {
    CHARYBDIS_LAYER_BASE = 0,
    CHARYBDIS_LAYER_LOWER = 1,
    CHARYBDIS_LAYER_RAISE = 2,
    CHARYBDIS_LAYER_MOUSE = 3,
    CHARYBDIS_LAYER_SYM = 4,
    CHARYBDIS_LAYER_NUM = 5,
    CHARYBDIS_LAYER_FUN = 6,
    CHARYBDIS_LAYER_BUTTON = 7,
    CHARYBDIS_LAYER_I3 = 8,
};

static const struct zmk_led_hsb base_color = {.h = 105, .s = 100, .b = 75};
static const struct zmk_led_hsb lower_color = {.h = 36, .s = 100, .b = 85};
static const struct zmk_led_hsb raise_color = {.h = 210, .s = 100, .b = 85};
static const struct zmk_led_hsb mouse_color = {.h = 0, .s = 100, .b = 85};
static const struct zmk_led_hsb button_color = {.h = 0, .s = 0, .b = 100};
static const struct zmk_led_hsb i3_color = {.h = 128, .s = 100, .b = 85};
static const struct zmk_led_hsb sym_color = {.h = 300, .s = 90, .b = 80};
static const struct zmk_led_hsb num_color = {.h = 52, .s = 95, .b = 85};
static const struct zmk_led_hsb fun_color = {.h = 270, .s = 90, .b = 85};

#define CHARYBDIS_LED_COUNT 18
#define ZONE_SIZE 3

#define ZONE_LEFT_OUTER 0
#define ZONE_LEFT_HOME 1
#define ZONE_LEFT_THUMBS 2
#define ZONE_RIGHT_THUMBS 3
#define ZONE_RIGHT_HOME 4
#define ZONE_RIGHT_OUTER 5

#if DT_NODE_EXISTS(DT_CHOSEN(zmk_underglow))
static const struct device *const led_strip = DEVICE_DT_GET(DT_CHOSEN(zmk_underglow));
#endif

static struct zmk_led_hsb active_layer_color(void) {
    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_BUTTON)) {
        return button_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_MOUSE)) {
        return mouse_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_I3)) {
        return i3_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_RAISE)) {
        return raise_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_LOWER)) {
        return lower_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_FUN)) {
        return fun_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_NUM)) {
        return num_color;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_SYM)) {
        return sym_color;
    }

    return base_color;
}

static struct led_rgb hsb_to_rgb(struct zmk_led_hsb color) {
    uint16_t hue = ((uint16_t)color.h * 360U) / 255U;
    uint8_t sat = color.s;
    uint8_t val = color.b;

    if (sat == 0) {
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

static struct led_rgb scale_rgb(struct led_rgb color, uint8_t percent) {
    return (struct led_rgb){
        .r = (uint8_t)(((uint16_t)color.r * percent) / 100U),
        .g = (uint8_t)(((uint16_t)color.g * percent) / 100U),
        .b = (uint8_t)(((uint16_t)color.b * percent) / 100U),
    };
}

static void fill_pixels(struct led_rgb *pixels, struct led_rgb color) {
    for (int i = 0; i < CHARYBDIS_LED_COUNT; i++) {
        pixels[i] = color;
    }
}

static void fill_zone(struct led_rgb *pixels, int zone, struct led_rgb color) {
    int start = zone * ZONE_SIZE;
    int end = start + ZONE_SIZE;

    for (int i = start; i < end && i < CHARYBDIS_LED_COUNT; i++) {
        pixels[i] = color;
    }
}

static int update_led_zones(void) {
#if DT_NODE_EXISTS(DT_CHOSEN(zmk_underglow))
    if (!device_is_ready(led_strip)) {
        return -ENODEV;
    }

    struct led_rgb pixels[CHARYBDIS_LED_COUNT];
    struct led_rgb base = hsb_to_rgb(base_color);
    struct led_rgb lower = hsb_to_rgb(lower_color);
    struct led_rgb raise = hsb_to_rgb(raise_color);
    struct led_rgb mouse = hsb_to_rgb(mouse_color);
    struct led_rgb button = hsb_to_rgb(button_color);
    struct led_rgb i3 = hsb_to_rgb(i3_color);
    struct led_rgb sym = hsb_to_rgb(sym_color);
    struct led_rgb num = hsb_to_rgb(num_color);
    struct led_rgb fun = hsb_to_rgb(fun_color);

    fill_pixels(pixels, base);

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_BUTTON)) {
        fill_pixels(pixels, button);
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_MOUSE)) {
        fill_pixels(pixels, scale_rgb(mouse, 20));
        fill_zone(pixels, ZONE_LEFT_THUMBS, mouse);
        fill_zone(pixels, ZONE_RIGHT_THUMBS, mouse);
        fill_zone(pixels, ZONE_RIGHT_HOME, scale_rgb(mouse, 70));
        fill_zone(pixels, ZONE_RIGHT_OUTER, mouse);
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_I3)) {
        fill_pixels(pixels, scale_rgb(i3, 18));
        fill_zone(pixels, ZONE_LEFT_HOME, i3);
        fill_zone(pixels, ZONE_RIGHT_HOME, i3);
        fill_zone(pixels, ZONE_LEFT_THUMBS, scale_rgb(i3, 70));
        fill_zone(pixels, ZONE_RIGHT_THUMBS, scale_rgb(i3, 70));
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_RAISE)) {
        fill_pixels(pixels, scale_rgb(raise, 18));
        fill_zone(pixels, ZONE_LEFT_OUTER, scale_rgb(raise, 70));
        fill_zone(pixels, ZONE_LEFT_HOME, raise);
        fill_zone(pixels, ZONE_LEFT_THUMBS, raise);
        fill_zone(pixels, ZONE_RIGHT_THUMBS, scale_rgb(raise, 70));
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_LOWER)) {
        fill_pixels(pixels, scale_rgb(lower, 18));
        fill_zone(pixels, ZONE_RIGHT_OUTER, scale_rgb(lower, 70));
        fill_zone(pixels, ZONE_RIGHT_HOME, lower);
        fill_zone(pixels, ZONE_RIGHT_THUMBS, lower);
        fill_zone(pixels, ZONE_LEFT_THUMBS, scale_rgb(lower, 70));
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_FUN)) {
        fill_pixels(pixels, scale_rgb(fun, 25));
        fill_zone(pixels, ZONE_LEFT_OUTER, fun);
        fill_zone(pixels, ZONE_RIGHT_OUTER, fun);
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_NUM)) {
        fill_pixels(pixels, scale_rgb(num, 18));
        fill_zone(pixels, ZONE_RIGHT_HOME, num);
        fill_zone(pixels, ZONE_RIGHT_OUTER, scale_rgb(num, 80));
        fill_zone(pixels, ZONE_RIGHT_THUMBS, scale_rgb(num, 60));
    } else if (zmk_keymap_layer_active(CHARYBDIS_LAYER_SYM)) {
        fill_pixels(pixels, scale_rgb(sym, 18));
        fill_zone(pixels, ZONE_LEFT_OUTER, scale_rgb(sym, 80));
        fill_zone(pixels, ZONE_RIGHT_OUTER, scale_rgb(sym, 80));
        fill_zone(pixels, ZONE_LEFT_THUMBS, scale_rgb(sym, 60));
        fill_zone(pixels, ZONE_RIGHT_THUMBS, scale_rgb(sym, 60));
    }

    return led_strip_update_rgb(led_strip, pixels, CHARYBDIS_LED_COUNT);
#else
    return -ENOTSUP;
#endif
}

static int layer_color_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);

    if (ev == NULL) {
        return 0;
    }

    struct zmk_led_hsb color = active_layer_color();
    int err = update_led_zones();

    if (err == -ENODEV || err == -ENOTSUP) {
        err = zmk_rgb_underglow_set_hsb(color);
    }

    if (err < 0) {
        LOG_WRN("layer color update failed: %d", err);
    } else {
        LOG_DBG("layer=%d state=%d color=%d/%d/%d", ev->layer, ev->state, color.h, color.s,
                color.b);
    }

    return 0;
}

ZMK_LISTENER(charybdis_layer_color, layer_color_listener);
ZMK_SUBSCRIPTION(charybdis_layer_color, zmk_layer_state_changed);

#endif
