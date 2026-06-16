/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_DISPLAY_STARTUP_H_
#define ZEPHYR_AUDIO_ANALYZER_DISPLAY_STARTUP_H_

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

int display_startup_init(void);
const struct device *display_startup_get_device(void);
void display_startup_get_capabilities(struct display_capabilities *capabilities);

#endif /* ZEPHYR_AUDIO_ANALYZER_DISPLAY_STARTUP_H_ */
