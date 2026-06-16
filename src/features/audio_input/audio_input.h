/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_INPUT_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_INPUT_H_

#include <stdint.h>

enum audio_input_mode {
	AUDIO_INPUT_MODE_MIC,
	AUDIO_INPUT_MODE_UART,
};

int audio_input_init(void);
int audio_input_set_mode(enum audio_input_mode mode);
enum audio_input_mode audio_input_get_mode(void);
int audio_input_read(uint16_t *sample);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_INPUT_H_ */
