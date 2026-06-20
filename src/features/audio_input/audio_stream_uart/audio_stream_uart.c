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
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define AUDIO_UART_NODE DT_CHOSEN(zephyr_audio_uart)

#define AUDIO_UART_SYNC_0 0xa5
#define AUDIO_UART_SYNC_1 0x5a
#define AUDIO_UART_MAX_FRAME_SAMPLES 128
#define AUDIO_UART_FRAME_QUEUE_DEPTH 8
#define AUDIO_UART_PREFILL_SAMPLES 256
#define AUDIO_UART_READ_TIMEOUT_MS 20
#define AUDIO_UART_STATS_INTERVAL_MS 1000
#define AUDIO_UART_MAX_SAMPLE 4095

enum uart_parse_state {
	UART_PARSE_SYNC_0,
	UART_PARSE_SYNC_1,
	UART_PARSE_SEQUENCE,
	UART_PARSE_COUNT,
	UART_PARSE_SAMPLE_LO,
	UART_PARSE_SAMPLE_HI,
};

struct uart_audio_frame {
	uint16_t samples[AUDIO_UART_MAX_FRAME_SAMPLES];
	uint8_t count;
	uint8_t sequence;
};

K_MSGQ_DEFINE(uart_frame_msgq, sizeof(struct uart_audio_frame),
	      AUDIO_UART_FRAME_QUEUE_DEPTH, 2);

static const struct device *const uart_dev = DEVICE_DT_GET(AUDIO_UART_NODE);
static enum uart_parse_state parse_state = UART_PARSE_SYNC_0;
static struct uart_audio_frame parser_frame;
static struct uart_audio_frame read_frame;
static uint8_t frame_samples_read;
static uint8_t read_frame_pos;
static uint8_t sample_lo;
static bool frame_has_invalid_sample;
static bool sequence_initialized;
static bool playback_started;
static uint8_t expected_sequence;
static atomic_t received_bytes;
static atomic_t valid_frames;
static atomic_t sequence_gaps;
static atomic_t queued_samples;
static atomic_t dropped_uart_samples;
static atomic_t read_timeouts;
static atomic_t invalid_samples;
static struct k_work_delayable stats_work;

BUILD_ASSERT(DT_NODE_HAS_STATUS(AUDIO_UART_NODE, okay),
	     "Set chosen zephyr,audio-uart to an enabled UART node");
BUILD_ASSERT(AUDIO_UART_FRAME_QUEUE_DEPTH * AUDIO_UART_MAX_FRAME_SAMPLES >=
	     AUDIO_UART_PREFILL_SAMPLES,
	     "UART frame queue must hold the startup prefill");

static void parser_reset(void)
{
	parse_state = UART_PARSE_SYNC_0;
	parser_frame.count = 0;
	frame_samples_read = 0;
	frame_has_invalid_sample = false;
}

static void track_sequence(uint8_t sequence)
{
	if (sequence_initialized && sequence != expected_sequence) {
		atomic_add(&sequence_gaps, (uint8_t)(sequence - expected_sequence));
	}

	expected_sequence = sequence + 1U;
	sequence_initialized = true;
}

static void submit_frame(void)
{
	track_sequence(parser_frame.sequence);

	if (frame_has_invalid_sample) {
		return;
	}

	atomic_inc(&valid_frames);
	if (k_msgq_put(&uart_frame_msgq, &parser_frame, K_NO_WAIT) != 0) {
		atomic_add(&dropped_uart_samples, parser_frame.count);
		return;
	}

	atomic_add(&queued_samples, parser_frame.count);
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
		if (byte == AUDIO_UART_SYNC_1) {
			parse_state = UART_PARSE_SEQUENCE;
		} else if (byte != AUDIO_UART_SYNC_0) {
			parse_state = UART_PARSE_SYNC_0;
		}
		break;
	case UART_PARSE_SEQUENCE:
		parser_frame.sequence = byte;
		parse_state = UART_PARSE_COUNT;
		break;
	case UART_PARSE_COUNT:
		parser_frame.count = byte;
		frame_samples_read = 0;
		frame_has_invalid_sample = false;

		if (parser_frame.count == 0 ||
		    parser_frame.count > AUDIO_UART_MAX_FRAME_SAMPLES) {
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
		parser_frame.samples[frame_samples_read] =
			((uint16_t)byte << 8) | sample_lo;
		if (parser_frame.samples[frame_samples_read] > AUDIO_UART_MAX_SAMPLE) {
			frame_has_invalid_sample = true;
			atomic_inc(&invalid_samples);
		}

		frame_samples_read++;
		if (frame_samples_read < parser_frame.count) {
			parse_state = UART_PARSE_SAMPLE_LO;
		} else {
			submit_frame();
			parser_reset();
		}
		break;
	default:
		parser_reset();
		break;
	}
}

static void uart_irq_callback(const struct device *dev, void *user_data)
{
	uint8_t bytes[32];
	int read;

	uart_irq_update(dev);
	ARG_UNUSED(user_data);

	if (!uart_irq_is_pending(dev) || !uart_irq_rx_ready(dev)) {
		return;
	}

	do {
		read = uart_fifo_read(dev, bytes, sizeof(bytes));
		if (read <= 0) {
			break;
		}

		atomic_add(&received_bytes, read);
		for (int i = 0; i < read; i++) {
			parse_byte(bytes[i]);
		}
	} while (read == (int)sizeof(bytes));
}

static void stats_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	printk("UART audio: received_bytes=%ld valid_frames=%ld sequence_gaps=%ld "
	       "queued_samples=%ld dropped_uart_samples=%ld read_timeouts=%ld "
	       "invalid_samples=%ld\n",
	       (long)atomic_get(&received_bytes), (long)atomic_get(&valid_frames),
	       (long)atomic_get(&sequence_gaps), (long)atomic_get(&queued_samples),
	       (long)atomic_get(&dropped_uart_samples), (long)atomic_get(&read_timeouts),
	       (long)atomic_get(&invalid_samples));

	k_work_reschedule(&stats_work, K_MSEC(AUDIO_UART_STATS_INTERVAL_MS));
}

int audio_stream_uart_init(void)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		return -ENODEV;
	}

	parser_reset();
	k_msgq_purge(&uart_frame_msgq);
	read_frame_pos = 0;
	read_frame.count = 0;
	playback_started = false;
	sequence_initialized = false;
	atomic_set(&received_bytes, 0);
	atomic_set(&valid_frames, 0);
	atomic_set(&sequence_gaps, 0);
	atomic_set(&queued_samples, 0);
	atomic_set(&dropped_uart_samples, 0);
	atomic_set(&read_timeouts, 0);
	atomic_set(&invalid_samples, 0);

	ret = uart_irq_callback_user_data_set(uart_dev, uart_irq_callback, NULL);
	if (ret != 0) {
		return ret;
	}

	uart_irq_rx_enable(uart_dev);
	k_work_init_delayable(&stats_work, stats_work_handler);
	k_work_schedule(&stats_work, K_MSEC(AUDIO_UART_STATS_INTERVAL_MS));

	return 0;
}

int audio_stream_uart_read(uint16_t *sample)
{
	int ret;

	if (!playback_started &&
	    atomic_get(&queued_samples) < AUDIO_UART_PREFILL_SAMPLES) {
		return -EAGAIN;
	}

	playback_started = true;
	if (read_frame_pos >= read_frame.count) {
		ret = k_msgq_get(&uart_frame_msgq, &read_frame,
				 K_MSEC(AUDIO_UART_READ_TIMEOUT_MS));
		if (ret != 0) {
			if (ret == -EAGAIN) {
				atomic_inc(&read_timeouts);
				playback_started = false;
			}
			return ret;
		}

		read_frame_pos = 0;
	}

	*sample = read_frame.samples[read_frame_pos++];
	atomic_dec(&queued_samples);
	return 0;
}

void audio_stream_uart_flush(void)
{
	uart_irq_rx_disable(uart_dev);
	k_msgq_purge(&uart_frame_msgq);
	parser_reset();
	read_frame_pos = 0;
	read_frame.count = 0;
	playback_started = false;
	sequence_initialized = false;
	atomic_set(&queued_samples, 0);
	uart_irq_rx_enable(uart_dev);
}
