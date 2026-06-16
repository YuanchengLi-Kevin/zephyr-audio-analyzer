/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_input/audio_stream_uart/audio_stream_uart.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define AUDIO_UART_NODE DT_CHOSEN(zephyr_audio_uart)

#define AUDIO_UART_SYNC_0 0xa5
#define AUDIO_UART_SYNC_1 0x5a
#define AUDIO_UART_MAX_FRAME_SAMPLES 128
#define AUDIO_UART_SAMPLE_QUEUE_DEPTH 1024
#define AUDIO_UART_READ_TIMEOUT_MS 20

enum uart_parse_state {
	UART_PARSE_SYNC_0,
	UART_PARSE_SYNC_1,
	UART_PARSE_SEQUENCE,
	UART_PARSE_COUNT,
	UART_PARSE_SAMPLE_LO,
	UART_PARSE_SAMPLE_HI,
};

K_MSGQ_DEFINE(uart_sample_msgq, sizeof(uint16_t), AUDIO_UART_SAMPLE_QUEUE_DEPTH, 2);

static const struct device *const uart_dev = DEVICE_DT_GET(AUDIO_UART_NODE);
static enum uart_parse_state parse_state = UART_PARSE_SYNC_0;
static uint8_t frame_count;
static uint8_t frame_samples_read;
static uint8_t sample_lo;
static uint32_t dropped_uart_samples;

BUILD_ASSERT(DT_NODE_HAS_STATUS(AUDIO_UART_NODE, okay),
	     "Set chosen zephyr,audio-uart to an enabled UART node");

static void parser_reset(void)
{
	parse_state = UART_PARSE_SYNC_0;
	frame_count = 0;
	frame_samples_read = 0;
}

static void submit_sample(uint16_t sample)
{
	if (k_msgq_put(&uart_sample_msgq, &sample, K_NO_WAIT) != 0) {
		dropped_uart_samples++;
	}
}

static void parse_byte(uint8_t byte)
{
	switch (parse_state) {
	case UART_PARSE_SYNC_0:
		if (byte == AUDIO_UART_SYNC_0) {
			parse_state = UART_PARSE_SYNC_1;
		}
		break;
	case UART_PARSE_SYNC_1:
		parse_state = byte == AUDIO_UART_SYNC_1 ? UART_PARSE_SEQUENCE : UART_PARSE_SYNC_0;
		break;
	case UART_PARSE_SEQUENCE:
		ARG_UNUSED(byte);
		parse_state = UART_PARSE_COUNT;
		break;
	case UART_PARSE_COUNT:
		frame_count = byte;
		frame_samples_read = 0;

		if (frame_count == 0 || frame_count > AUDIO_UART_MAX_FRAME_SAMPLES) {
			parser_reset();
		} else {
			parse_state = UART_PARSE_SAMPLE_LO;
		}
		break;
	case UART_PARSE_SAMPLE_LO:
		sample_lo = byte;
		parse_state = UART_PARSE_SAMPLE_HI;
		break;
	case UART_PARSE_SAMPLE_HI:
		submit_sample(((uint16_t)byte << 8) | sample_lo);
		frame_samples_read++;
		parse_state = frame_samples_read < frame_count ? UART_PARSE_SAMPLE_LO : UART_PARSE_SYNC_0;
		break;
	default:
		parser_reset();
		break;
	}
}

static void uart_irq_callback(const struct device *dev, void *user_data)
{
	uint8_t bytes[32];

	ARG_UNUSED(user_data);

	uart_irq_update(dev);
	while (uart_irq_rx_ready(dev)) {
		int read = uart_fifo_read(dev, bytes, sizeof(bytes));

		if (read <= 0) {
			break;
		}

		for (int i = 0; i < read; i++) {
			parse_byte(bytes[i]);
		}
	}
}

int audio_stream_uart_init(void)
{
	if (!device_is_ready(uart_dev)) {
		return -ENODEV;
	}

	parser_reset();
	k_msgq_purge(&uart_sample_msgq);
	uart_irq_callback_user_data_set(uart_dev, uart_irq_callback, NULL);
	uart_irq_rx_enable(uart_dev);

	return 0;
}

int audio_stream_uart_read(uint16_t *sample)
{
	return k_msgq_get(&uart_sample_msgq, sample, K_MSEC(AUDIO_UART_READ_TIMEOUT_MS));
}

void audio_stream_uart_flush(void)
{
	uart_irq_rx_disable(uart_dev);
	k_msgq_purge(&uart_sample_msgq);
	parser_reset();
	uart_irq_rx_enable(uart_dev);
}
