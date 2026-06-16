/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/display_spectrum/display_spectrum.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "features/display_startup/display_startup.h"

#define SPECTRUM_STACK_SIZE 2048
#define SPECTRUM_PRIORITY 7
#define SPECTRUM_QUEUE_DEPTH 2
#define SPECTRUM_FRAME_INTERVAL_MS 33U

#define SPECTRUM_COLOR_BACKGROUND 0x0000
#define SPECTRUM_COLOR_GRID 0x2104
#define SPECTRUM_COLOR_BAR 0x07e0

#define SPECTRUM_MAX_WIDTH 320U
#define SPECTRUM_MARGIN_X 8U
#define SPECTRUM_MARGIN_TOP 8U
#define SPECTRUM_MARGIN_BOTTOM 14U
#define SPECTRUM_BAR_GAP 2U
#define SPECTRUM_MAX_BAR_WIDTH \
	((SPECTRUM_MAX_WIDTH - (SPECTRUM_MARGIN_X * 2U)) / DISPLAY_SPECTRUM_BARS)
#define SPECTRUM_MAX_HEIGHT 240U

struct spectrum_frame {
	uint16_t bars[DISPLAY_SPECTRUM_BARS];
};

K_MSGQ_DEFINE(spectrum_msgq, sizeof(struct spectrum_frame), SPECTRUM_QUEUE_DEPTH, 4);
K_THREAD_STACK_DEFINE(spectrum_stack, SPECTRUM_STACK_SIZE);

static struct k_thread spectrum_thread_data;
static const struct device *display_dev;
static struct display_capabilities display_caps;
static uint16_t line_buffer[SPECTRUM_MAX_WIDTH];
static uint16_t rect_buffer[SPECTRUM_MAX_BAR_WIDTH * SPECTRUM_MAX_HEIGHT];
static uint16_t previous_heights[DISPLAY_SPECTRUM_BARS];
static uint32_t last_frame_ms;
static bool spectrum_started;

static uint32_t abs32(int32_t value)
{
	return value < 0 ? (uint32_t)-value : (uint32_t)value;
}

static uint16_t graph_width(void)
{
	return MIN(display_caps.x_resolution, SPECTRUM_MAX_WIDTH);
}

static uint16_t graph_height(void)
{
	if (display_caps.y_resolution <= SPECTRUM_MARGIN_TOP + SPECTRUM_MARGIN_BOTTOM) {
		return 0U;
	}

	return display_caps.y_resolution - SPECTRUM_MARGIN_TOP - SPECTRUM_MARGIN_BOTTOM;
}

static int write_solid_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
			    uint16_t color)
{
	uint32_t pixel_count = (uint32_t)width * height;
	struct display_buffer_descriptor desc = {
		.buf_size = width * sizeof(line_buffer[0]),
		.width = width,
		.height = 1,
		.pitch = width,
	};

	if (width == 0U || height == 0U) {
		return 0;
	}

	if (width > ARRAY_SIZE(line_buffer)) {
		return -EINVAL;
	}

	if (pixel_count <= ARRAY_SIZE(rect_buffer)) {
		struct display_buffer_descriptor rect_desc = {
			.buf_size = pixel_count * sizeof(rect_buffer[0]),
			.width = width,
			.height = height,
			.pitch = width,
		};

		for (uint32_t i = 0; i < pixel_count; i++) {
			rect_buffer[i] = color;
		}

		return display_write(display_dev, x, y, &rect_desc, rect_buffer);
	}

	for (uint16_t i = 0; i < width; i++) {
		line_buffer[i] = color;
	}

	for (uint16_t row = 0; row < height; row++) {
		int ret = display_write(display_dev, x, y + row, &desc, line_buffer);

		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static int draw_background(void)
{
	uint16_t width = graph_width();
	uint16_t height = display_caps.y_resolution;
	int ret = write_solid_rect(0, 0, width, height, SPECTRUM_COLOR_BACKGROUND);

	if (ret != 0) {
		return ret;
	}

	uint16_t graph_bottom = SPECTRUM_MARGIN_TOP + graph_height();

	return write_solid_rect(SPECTRUM_MARGIN_X, graph_bottom, width - (SPECTRUM_MARGIN_X * 2U),
				1U, SPECTRUM_COLOR_GRID);
}

static int draw_bar(uint16_t index, uint16_t height, uint16_t old_height)
{
	uint16_t width = graph_width();
	uint16_t available_width = width - (SPECTRUM_MARGIN_X * 2U);
	uint16_t slot_width = available_width / DISPLAY_SPECTRUM_BARS;
	uint16_t bar_width = slot_width > SPECTRUM_BAR_GAP ? slot_width - SPECTRUM_BAR_GAP : 1U;
	uint16_t max_height = graph_height();
	uint16_t graph_bottom = SPECTRUM_MARGIN_TOP + max_height;
	uint16_t x = SPECTRUM_MARGIN_X + (index * slot_width);
	int ret;

	height = MIN(height, max_height);
	old_height = MIN(old_height, max_height);

	if (height < old_height) {
		ret = write_solid_rect(x, graph_bottom - old_height, bar_width,
				       old_height - height, SPECTRUM_COLOR_BACKGROUND);
		if (ret != 0) {
			return ret;
		}
	} else if (height > old_height) {
		ret = write_solid_rect(x, graph_bottom - height, bar_width, height - old_height,
				       SPECTRUM_COLOR_BAR);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

static void draw_frame(const struct spectrum_frame *frame)
{
	uint16_t max_height = graph_height();

	for (uint16_t i = 0; i < DISPLAY_SPECTRUM_BARS; i++) {
		uint16_t target = frame->bars[i];
		uint16_t current = previous_heights[i];

		if (target < current && current > 0U) {
			uint16_t decay = MAX(current / 5U, 1U);

			target = current > decay ? MAX(target, current - decay) : target;
		}

		target = MIN(target, max_height);

		if (target == current) {
			continue;
		}

		if (draw_bar(i, target, current) != 0) {
			printk("Spectrum draw failed\n");
			return;
		}

		previous_heights[i] = target;
	}
}

static void spectrum_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	struct spectrum_frame frame;

	if (draw_background() != 0) {
		printk("Spectrum background draw failed\n");
	}

	while (true) {
		k_msgq_get(&spectrum_msgq, &frame, K_FOREVER);
		draw_frame(&frame);
	}
}

int display_spectrum_start(void)
{
	if (spectrum_started) {
		return 0;
	}

	display_dev = display_startup_get_device();
	if (display_dev == NULL || !device_is_ready(display_dev)) {
		return -ENODEV;
	}

	display_startup_get_capabilities(&display_caps);
	if (display_caps.x_resolution < (SPECTRUM_MARGIN_X * 2U) + DISPLAY_SPECTRUM_BARS ||
	    display_caps.y_resolution <= SPECTRUM_MARGIN_TOP + SPECTRUM_MARGIN_BOTTOM) {
		return -EINVAL;
	}

	memset(previous_heights, 0, sizeof(previous_heights));

	k_thread_create(&spectrum_thread_data, spectrum_stack,
			K_THREAD_STACK_SIZEOF(spectrum_stack), spectrum_thread, NULL, NULL, NULL,
			SPECTRUM_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&spectrum_thread_data, "spectrum");
	spectrum_started = true;

	return 0;
}

void display_spectrum_submit_fft(const int32_t real[AUDIO_FFT_SIZE],
				 const int32_t imag[AUDIO_FFT_SIZE])
{
	struct spectrum_frame frame = { 0 };
	uint32_t magnitudes[DISPLAY_SPECTRUM_BARS] = { 0 };
	uint32_t peak = 1U;
	uint32_t now_ms;
	uint16_t max_height;

	if (!spectrum_started) {
		return;
	}

	now_ms = k_uptime_get_32();
	if (last_frame_ms != 0U && (now_ms - last_frame_ms) < SPECTRUM_FRAME_INTERVAL_MS) {
		return;
	}
	last_frame_ms = now_ms;

	for (uint16_t bar = 0; bar < DISPLAY_SPECTRUM_BARS; bar++) {
		uint16_t start_bin = 1U + (bar * ((AUDIO_FFT_SIZE / 2U) - 1U)) /
						   DISPLAY_SPECTRUM_BARS;
		uint16_t end_bin = 1U + ((bar + 1U) * ((AUDIO_FFT_SIZE / 2U) - 1U)) /
						 DISPLAY_SPECTRUM_BARS;
		uint32_t sum = 0U;
		uint16_t count = 0U;

		for (uint16_t bin = start_bin; bin < end_bin; bin++) {
			sum += abs32(real[bin]) + abs32(imag[bin]);
			count++;
		}

		magnitudes[bar] = count > 0U ? sum / count : 0U;
		peak = MAX(peak, magnitudes[bar]);
	}

	max_height = graph_height();
	for (uint16_t bar = 0; bar < DISPLAY_SPECTRUM_BARS; bar++) {
		frame.bars[bar] = (uint16_t)((magnitudes[bar] * max_height) / peak);
	}

	if (k_msgq_put(&spectrum_msgq, &frame, K_NO_WAIT) != 0) {
		k_msgq_purge(&spectrum_msgq);
		(void)k_msgq_put(&spectrum_msgq, &frame, K_NO_WAIT);
	}
}
