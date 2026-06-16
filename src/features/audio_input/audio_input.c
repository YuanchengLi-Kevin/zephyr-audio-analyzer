/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_input/audio_input.h"

#include <errno.h>

#include <zephyr/sys/atomic.h>

#include "features/audio_input/audio_stream_mic/audio_stream_mic.h"
#include "features/audio_input/audio_stream_uart/audio_stream_uart.h"

static atomic_t current_mode = ATOMIC_INIT(AUDIO_INPUT_MODE_MIC);

int audio_input_init(void)
{
	int ret = audio_stream_mic_init();

	if (ret != 0) {
		return ret;
	}

	return audio_stream_uart_init();
}

int audio_input_set_mode(enum audio_input_mode mode)
{
	switch (mode) {
	case AUDIO_INPUT_MODE_MIC:
		atomic_set(&current_mode, mode);
		audio_stream_uart_flush();
		return 0;
	case AUDIO_INPUT_MODE_UART:
		atomic_set(&current_mode, mode);
		audio_stream_uart_flush();
		return 0;
	default:
		return -EINVAL;
	}
}

enum audio_input_mode audio_input_get_mode(void)
{
	return (enum audio_input_mode)atomic_get(&current_mode);
}

int audio_input_read(uint16_t *sample)
{
	switch (audio_input_get_mode()) {
	case AUDIO_INPUT_MODE_MIC:
		return audio_stream_mic_read(sample);
	case AUDIO_INPUT_MODE_UART:
		return audio_stream_uart_read(sample);
	default:
		return -EINVAL;
	}
}
