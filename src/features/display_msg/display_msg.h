/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_DISPLAY_MSG_H_
#define ZEPHYR_AUDIO_ANALYZER_DISPLAY_MSG_H_

#include "features/audio_input/audio_input.h"

int display_msg_init(void);
void display_msg_show_input_mode(enum audio_input_mode mode);

#endif /* ZEPHYR_AUDIO_ANALYZER_DISPLAY_MSG_H_ */
