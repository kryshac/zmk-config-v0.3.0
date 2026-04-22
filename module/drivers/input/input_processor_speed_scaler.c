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
#define MAX_CODES 8
#define SPEED_SCALE_FP 65536U

struct speed_scaler_config {
    uint8_t type;
    bool debug_log;
    uint16_t debug_log_interval_ms;
    size_t codes_len;
    const uint16_t *codes;
    uint16_t low_speed;
    uint16_t high_speed;
    uint16_t timeout_ms;
    uint16_t minimum_dt_ms;
    uint8_t curve_exponent;
    uint8_t slow_mul;
    uint8_t slow_div;
    uint8_t fast_mul;
    uint8_t fast_div;
};

struct code_speed_state {
    uint32_t last_event_ms;
    uint16_t last_speed;
    int32_t remainder;
    int8_t last_sign;
};

struct listener_speed_state {
    struct code_speed_state codes[MAX_CODES];
    uint32_t last_debug_log_ms;
};

struct speed_scaler_data {
    struct listener_speed_state listeners[LISTENER_COUNT];
};

static bool event_matches(const struct speed_scaler_config *cfg, const struct input_event *event,
                          size_t *code_index) {
    if (event->type != cfg->type) {
        return false;
    }

    for (size_t i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            if (code_index != NULL) {
                *code_index = i;
            }

            return true;
        }
    }

    return false;
}

static uint32_t scale_ratio_fp(uint32_t mul, uint32_t div) {
    return (mul * SPEED_SCALE_FP) / div;
}

static uint32_t curve_scale_fp(const struct speed_scaler_config *cfg, uint32_t speed) {
    uint32_t slow = scale_ratio_fp(cfg->slow_mul, cfg->slow_div);
    uint32_t fast = scale_ratio_fp(cfg->fast_mul, cfg->fast_div);

    if (speed <= cfg->low_speed) {
        return slow;
    }

    if (speed >= cfg->high_speed) {
        return fast;
    }

    uint32_t range = cfg->high_speed - cfg->low_speed;
    uint32_t pos = speed - cfg->low_speed;
    uint32_t t = ((uint64_t)pos * SPEED_SCALE_FP) / range;
    uint32_t curve = t;

    for (uint8_t i = 1; i < cfg->curve_exponent; i++) {
        curve = ((uint64_t)curve * t) / SPEED_SCALE_FP;
    }

    int64_t scale = slow + (((int64_t)fast - slow) * curve) / SPEED_SCALE_FP;

    return scale <= 0 ? 0 : (uint32_t)scale;
}

static int scale_val(struct input_event *event, uint32_t scale_fp, int32_t *remainder) {
    int64_t value = (int64_t)event->value * scale_fp;

    if (remainder != NULL) {
        value += *remainder;
    }

    int16_t scaled = value / SPEED_SCALE_FP;

    if (remainder != NULL) {
        *remainder = value - ((int64_t)scaled * SPEED_SCALE_FP);
    }

    event->value = scaled;

    return ZMK_INPUT_PROC_CONTINUE;
}

static int8_t value_sign(int16_t value) {
    if (value > 0) {
        return 1;
    }

    if (value < 0) {
        return -1;
    }

    return 0;
}

static const char *axis_name(uint16_t code) {
    switch (code) {
    case INPUT_REL_X:
        return "x";
    case INPUT_REL_Y:
        return "y";
    default:
        return "?";
    }
}

static int speed_scaler_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t param1, uint32_t param2,
                                     struct zmk_input_processor_state *state) {
    const struct speed_scaler_config *cfg = dev->config;
    struct speed_scaler_data *data = dev->data;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    size_t code_index = 0;

    if (!event_matches(cfg, event, &code_index)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (state == NULL || state->input_device_index >= LISTENER_COUNT) {
        return scale_val(event, scale_ratio_fp(cfg->slow_mul, cfg->slow_div), NULL);
    }

    struct code_speed_state *code_state =
        &data->listeners[state->input_device_index].codes[code_index];
    uint32_t now = k_uptime_get_32();
    uint32_t dt = now - code_state->last_event_ms;
    int16_t raw_value = event->value;
    int8_t raw_sign = value_sign(raw_value);

    if (code_state->last_event_ms == 0 || dt > cfg->timeout_ms) {
        code_state->last_speed = 0;
        code_state->remainder = 0;
        code_state->last_sign = 0;
        dt = cfg->timeout_ms + 1U;
    } else if (raw_sign != 0 && code_state->last_sign != 0 &&
               raw_sign != code_state->last_sign) {
        code_state->remainder = 0;
    }

    if (raw_sign != 0) {
        code_state->last_sign = raw_sign;
    }

    code_state->last_event_ms = now;

    if (dt <= cfg->timeout_ms) {
        uint32_t effective_dt = MAX(dt, (uint32_t)cfg->minimum_dt_ms);
        uint32_t magnitude =
            (uint32_t)(event->value < 0 ? -(int32_t)event->value : (int32_t)event->value);
        uint32_t sample_speed = (magnitude * 1000U) / effective_dt;
        code_state->last_speed = (code_state->last_speed == 0)
                                   ? sample_speed
                                   : (uint16_t)((code_state->last_speed + sample_speed) / 2U);
    }

    uint32_t scale_fp = curve_scale_fp(cfg, code_state->last_speed);
    uint32_t scale_permill = ((uint64_t)scale_fp * 1000U) / SPEED_SCALE_FP;
    int ret = scale_val(event, scale_fp, &code_state->remainder);

    bool should_log = cfg->debug_log;

    if (should_log && cfg->debug_log_interval_ms > 0) {
        struct listener_speed_state *listener = &data->listeners[state->input_device_index];
        uint32_t since_debug_log = now - listener->last_debug_log_ms;

        should_log = listener->last_debug_log_ms == 0 ||
                     since_debug_log >= cfg->debug_log_interval_ms;

        if (should_log) {
            listener->last_debug_log_ms = now;
        }
    }

    if (should_log) {
        LOG_INF("TB_SPEED ms=%u listener=%d axis=%s code=%d raw=%d scaled=%d dt=%u speed=%u "
                "scale_permill=%u remainder=%d",
                now, state->input_device_index, axis_name(event->code), event->code, raw_value,
                event->value, dt, code_state->last_speed, scale_permill, code_state->remainder);
    } else {
        LOG_DBG("listener=%d code=%d raw=%d scaled=%d speed=%d scale_permill=%d",
                state->input_device_index, event->code, raw_value, event->value,
                code_state->last_speed, scale_permill);
    }

    return ret;
}

static struct zmk_input_processor_driver_api speed_scaler_driver_api = {
    .handle_event = speed_scaler_handle_event,
};

#define SPEED_SCALER_INST(n)                                                                       \
    BUILD_ASSERT(DT_INST_PROP(n, low_speed) <= DT_INST_PROP(n, high_speed),                       \
                 "low-speed must be <= high-speed");                                              \
    BUILD_ASSERT(DT_INST_PROP(n, slow_divisor) > 0 && DT_INST_PROP(n, fast_divisor) > 0,          \
                 "speed scaler divisors must be > 0");                                            \
    BUILD_ASSERT(DT_INST_PROP(n, curve_exponent) > 0, "curve-exponent must be > 0");              \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, codes) <= MAX_CODES,                                          \
                 "speed scaler code count exceeds MAX_CODES");                                    \
    static const uint16_t speed_scaler_codes_##n[] = DT_INST_PROP(n, codes);                      \
    static struct speed_scaler_data speed_scaler_data_##n;                                         \
    static const struct speed_scaler_config speed_scaler_config_##n = {                            \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .debug_log = DT_INST_PROP(n, debug_log),                                                   \
        .debug_log_interval_ms = DT_INST_PROP(n, debug_log_interval_ms),                           \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = speed_scaler_codes_##n,                                                           \
        .low_speed = DT_INST_PROP(n, low_speed),                                                   \
        .high_speed = DT_INST_PROP(n, high_speed),                                                 \
        .timeout_ms = DT_INST_PROP(n, timeout_ms),                                                 \
        .minimum_dt_ms = DT_INST_PROP(n, minimum_dt_ms),                                           \
        .curve_exponent = DT_INST_PROP(n, curve_exponent),                                         \
        .slow_mul = DT_INST_PROP(n, slow_multiplier),                                              \
        .slow_div = DT_INST_PROP(n, slow_divisor),                                                 \
        .fast_mul = DT_INST_PROP(n, fast_multiplier),                                              \
        .fast_div = DT_INST_PROP(n, fast_divisor),                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &speed_scaler_data_##n, &speed_scaler_config_##n,        \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &speed_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPEED_SCALER_INST)
