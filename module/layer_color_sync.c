/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>
#endif

#include "layer_color.h"

#define DT_DRV_COMPAT zmk_behavior_led_layer_sync

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static uint8_t rendered_layer = UINT8_MAX;

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static uint8_t active_visual_layer(void) {
    /* Keep the base palette visible while using the H/S/B tuning layer. */
    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_SNI)) {
        return CHARYBDIS_LAYER_BASE;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_SCR)) {
        return CHARYBDIS_LAYER_SCR;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_POI)) {
        return CHARYBDIS_LAYER_POI;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_MED)) {
        return CHARYBDIS_LAYER_MED;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_NAV)) {
        return CHARYBDIS_LAYER_NAV;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_SYM)) {
        return CHARYBDIS_LAYER_SYM;
    }

    if (zmk_keymap_layer_active(CHARYBDIS_LAYER_NUFU)) {
        return CHARYBDIS_LAYER_NUFU;
    }

    return CHARYBDIS_LAYER_BASE;
}

#endif

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_led_layer_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    uint8_t layer = (uint8_t)binding->param1;
    if (rendered_layer == layer) {
        return 0;
    }

    rendered_layer = layer;
    return charybdis_layer_color_apply(layer);
}

static int on_led_layer_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return 0;
}

static const struct behavior_driver_api behavior_led_layer_sync_driver_api = {
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
    .binding_pressed = on_led_layer_binding_pressed,
    .binding_released = on_led_layer_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_led_layer_sync_driver_api);

#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static int sync_led_layer_to_peripherals(uint8_t layer) {
    struct zmk_behavior_binding binding = {
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
        .behavior_dev = DEVICE_DT_NAME(DT_INST(0, DT_DRV_COMPAT)),
#else
        .behavior_dev = "",
#endif
        .param1 = layer,
        .param2 = 0,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };

#if defined(CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS)
    for (uint8_t source = 0; source < CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS; source++) {
        zmk_split_central_invoke_behavior(source, &binding, event, true);
    }
#endif

    return 0;
}

static int layer_color_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);

    if (ev == NULL) {
        return 0;
    }

    uint8_t layer = active_visual_layer();
    if (rendered_layer == layer) {
        return 0;
    }

    rendered_layer = layer;
    charybdis_layer_color_apply(layer);
    sync_led_layer_to_peripherals(layer);
    return 0;
}

ZMK_LISTENER(charybdis_layer_color, layer_color_listener);
ZMK_SUBSCRIPTION(charybdis_layer_color, zmk_layer_state_changed);

#endif
