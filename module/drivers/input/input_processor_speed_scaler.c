/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_speed_scaler

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define INPUT_LISTENER_COMPAT zmk_input_listener
#define LISTENER_COUNT MAX(1, DT_NUM_INST_STATUS_OKAY(INPUT_LISTENER_COMPAT))

struct speed_scaler_config {
    uint8_t type;
    size_t codes_len;
    const uint16_t *codes;
    uint16_t low_speed;
    uint16_t high_speed;
    uint16_t timeout_ms;
    uint16_t minimum_dt_ms;
    uint8_t slow_mul;
    uint8_t slow_div;
    uint8_t medium_mul;
    uint8_t medium_div;
    uint8_t fast_mul;
    uint8_t fast_div;
};

struct listener_speed_state {
    uint32_t last_event_ms;
    uint16_t last_speed;
};

struct speed_scaler_data {
    struct listener_speed_state listeners[LISTENER_COUNT];
};

static bool event_matches(const struct speed_scaler_config *cfg, const struct input_event *event) {
    if (event->type != cfg->type) {
        return false;
    }

    for (size_t i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            return true;
        }
    }

    return false;
}

static int scale_val(struct input_event *event, uint32_t mul, uint32_t div,
                     struct zmk_input_processor_state *state) {
    int32_t value_mul = event->value * (int32_t)mul;

    if (state != NULL && state->remainder != NULL) {
        value_mul += *state->remainder;
    }

    int16_t scaled = value_mul / (int32_t)div;

    if (state != NULL && state->remainder != NULL) {
        *state->remainder = value_mul - (scaled * (int32_t)div);
    }

    event->value = scaled;

    return ZMK_INPUT_PROC_CONTINUE;
}

static int speed_scaler_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t param1, uint32_t param2,
                                     struct zmk_input_processor_state *state) {
    const struct speed_scaler_config *cfg = dev->config;
    struct speed_scaler_data *data = dev->data;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (!event_matches(cfg, event)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (state == NULL || state->input_device_index >= LISTENER_COUNT) {
        return scale_val(event, cfg->medium_mul, cfg->medium_div, state);
    }

    struct listener_speed_state *listener = &data->listeners[state->input_device_index];
    uint32_t now = k_uptime_get_32();
    uint32_t dt = now - listener->last_event_ms;

    if (listener->last_event_ms == 0 || dt > cfg->timeout_ms) {
        listener->last_speed = 0;
        dt = cfg->timeout_ms + 1U;
    }

    listener->last_event_ms = now;

    if (dt <= cfg->timeout_ms) {
        uint32_t effective_dt = MAX(dt, (uint32_t)cfg->minimum_dt_ms);
        uint32_t magnitude =
            (uint32_t)(event->value < 0 ? -(int32_t)event->value : (int32_t)event->value);
        uint32_t sample_speed = (magnitude * 1000U) / effective_dt;
        listener->last_speed = (listener->last_speed == 0)
                                   ? sample_speed
                                   : (uint16_t)((listener->last_speed + sample_speed) / 2U);
    }

    uint8_t mul = cfg->slow_mul;
    uint8_t div = cfg->slow_div;
    const char *band = "slow";

    if (listener->last_speed >= cfg->high_speed) {
        mul = cfg->fast_mul;
        div = cfg->fast_div;
        band = "fast";
    } else if (listener->last_speed >= cfg->low_speed) {
        mul = cfg->medium_mul;
        div = cfg->medium_div;
        band = "medium";
    }

    LOG_DBG("listener=%d code=%d value=%d speed=%d band=%s scale=%d/%d",
            state->input_device_index, event->code, event->value, listener->last_speed, band, mul,
            div);

    return scale_val(event, mul, div, state);
}

static struct zmk_input_processor_driver_api speed_scaler_driver_api = {
    .handle_event = speed_scaler_handle_event,
};

#define SPEED_SCALER_INST(n)                                                                       \
    BUILD_ASSERT(DT_INST_PROP(n, low_speed) <= DT_INST_PROP(n, high_speed),                       \
                 "low-speed must be <= high-speed");                                              \
    BUILD_ASSERT(DT_INST_PROP(n, slow_divisor) > 0 && DT_INST_PROP(n, medium_divisor) > 0 &&     \
                     DT_INST_PROP(n, fast_divisor) > 0,                                            \
                 "speed scaler divisors must be > 0");                                            \
    static const uint16_t speed_scaler_codes_##n[] = DT_INST_PROP(n, codes);                      \
    static struct speed_scaler_data speed_scaler_data_##n;                                         \
    static const struct speed_scaler_config speed_scaler_config_##n = {                            \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = speed_scaler_codes_##n,                                                           \
        .low_speed = DT_INST_PROP(n, low_speed),                                                   \
        .high_speed = DT_INST_PROP(n, high_speed),                                                 \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
        .minimum_dt_ms = DT_INST_PROP(n, minimum_dt_ms),                                           \
        .slow_mul = DT_INST_PROP(n, slow_multiplier),                                              \
        .slow_div = DT_INST_PROP(n, slow_divisor),                                                 \
        .medium_mul = DT_INST_PROP(n, medium_multiplier),                                          \
        .medium_div = DT_INST_PROP(n, medium_divisor),                                             \
        .fast_mul = DT_INST_PROP(n, fast_multiplier),                                              \
        .fast_div = DT_INST_PROP(n, fast_divisor),                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &speed_scaler_data_##n, &speed_scaler_config_##n,        \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &speed_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPEED_SCALER_INST)
