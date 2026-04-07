/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include <drivers/behavior.h>

#include "layer_color.h"

#define DT_DRV_COMPAT zmk_behavior_led_hsb_adjust

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_led_hsb_adjust_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    uint8_t control = (uint8_t)binding->param1;
    return charybdis_layer_color_adjust(control);
}

static int on_led_hsb_adjust_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return 0;
}

static const struct behavior_driver_api behavior_led_hsb_adjust_driver_api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_pressed = on_led_hsb_adjust_pressed,
    .binding_released = on_led_hsb_adjust_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_led_hsb_adjust_driver_api);

#endif
