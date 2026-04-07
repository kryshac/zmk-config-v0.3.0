/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/sys/util.h>

#include "layer_color.h"

int charybdis_layer_color_apply(uint8_t layer) {
    ARG_UNUSED(layer);
    return -ENOTSUP;
}

int charybdis_layer_color_adjust(uint8_t control) {
    ARG_UNUSED(control);
    return -ENOTSUP;
}
