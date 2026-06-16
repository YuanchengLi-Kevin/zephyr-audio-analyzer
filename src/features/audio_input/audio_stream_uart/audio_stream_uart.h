/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_UART_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_UART_H_

#include <stdint.h>

int audio_stream_uart_init(void);
int audio_stream_uart_read(uint16_t *sample);
void audio_stream_uart_flush(void);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_UART_H_ */
