/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/display_msg/display_msg.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "features/display_startup/display_startup.h"

#define MSG_COLOR_BACKGROUND 0x0000
#define MSG_COLOR_MIC 0xffff
#define MSG_COLOR_UART 0x07ff
#define MSG_GLYPH_WIDTH 5U
#define MSG_GLYPH_HEIGHT 7U
#define MSG_GLYPH_SPACING 1U
#define MSG_SCALE 1U
#define MSG_PADDING_X 2U
#define MSG_PADDING_Y 1U
#define MSG_MAX_WIDTH 80U

static const struct device *display_dev;
static struct display_capabilities display_caps;
static struct k_mutex msg_lock;
static bool msg_ready;
static uint16_t line_buffer[MSG_MAX_WIDTH];

static const uint8_t glyph_space[MSG_GLYPH_HEIGHT] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t glyph_a[MSG_GLYPH_HEIGHT] = {
	0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11,
};

static const uint8_t glyph_c[MSG_GLYPH_HEIGHT] = {
	0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f,
};

static const uint8_t glyph_i[MSG_GLYPH_HEIGHT] = {
	0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f,
};

static const uint8_t glyph_m[MSG_GLYPH_HEIGHT] = {
	0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11,
};

static const uint8_t glyph_r[MSG_GLYPH_HEIGHT] = {
	0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11,
};

static const uint8_t glyph_t[MSG_GLYPH_HEIGHT] = {
	0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
};

static const uint8_t glyph_u[MSG_GLYPH_HEIGHT] = {
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e,
};

static const uint8_t *glyph_for_char(char c)
{
	switch (c) {
	case 'A':
		return glyph_a;
	case 'C':
		return glyph_c;
	case 'I':
		return glyph_i;
	case 'M':
		return glyph_m;
	case 'R':
		return glyph_r;
	case 'T':
		return glyph_t;
	case 'U':
		return glyph_u;
	default:
		return glyph_space;
	}
}

static int write_solid_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
			    uint16_t color)
{
	struct display_buffer_descriptor desc = {
		.buf_size = width * sizeof(line_buffer[0]),
		.width = width,
		.height = 1,
		.pitch = width,
	};

	if (width == 0U || height == 0U || width > ARRAY_SIZE(line_buffer)) {
		return -EINVAL;
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

static int draw_char(uint16_t x, uint16_t y, char c, uint16_t color)
{
	const uint8_t *glyph = glyph_for_char(c);

	for (uint16_t row = 0; row < MSG_GLYPH_HEIGHT; row++) {
		for (uint16_t col = 0; col < MSG_GLYPH_WIDTH; col++) {
			if ((glyph[row] & BIT(MSG_GLYPH_WIDTH - 1U - col)) == 0U) {
				continue;
			}

			int ret = write_solid_rect(x + (col * MSG_SCALE), y + (row * MSG_SCALE),
						   MSG_SCALE, MSG_SCALE, color);

			if (ret != 0) {
				return ret;
			}
		}
	}

	return 0;
}

static int draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color)
{
	for (size_t i = 0; text[i] != '\0'; i++) {
		int ret = draw_char(x, y, text[i], color);

		if (ret != 0) {
			return ret;
		}

		x += (MSG_GLYPH_WIDTH + MSG_GLYPH_SPACING) * MSG_SCALE;
	}

	return 0;
}

int display_msg_init(void)
{
	display_dev = display_startup_get_device();
	if (display_dev == NULL || !device_is_ready(display_dev)) {
		return -ENODEV;
	}

	display_startup_get_capabilities(&display_caps);
	k_mutex_init(&msg_lock);
	msg_ready = true;

	return 0;
}

void display_msg_show_input_mode(enum audio_input_mode mode)
{
	const char *text = mode == AUDIO_INPUT_MODE_UART ? "UART" : "MIC";
	uint16_t color = mode == AUDIO_INPUT_MODE_UART ? MSG_COLOR_UART : MSG_COLOR_MIC;
	uint16_t clear_width = MIN(display_caps.x_resolution, MSG_MAX_WIDTH);
	uint16_t height = (MSG_GLYPH_HEIGHT * MSG_SCALE) + (MSG_PADDING_Y * 2U);

	if (!msg_ready || display_caps.x_resolution == 0U) {
		return;
	}

	k_mutex_lock(&msg_lock, K_FOREVER);
	(void)write_solid_rect(0, 0, clear_width, height, MSG_COLOR_BACKGROUND);
	(void)draw_text(MSG_PADDING_X, MSG_PADDING_Y, text, color);
	k_mutex_unlock(&msg_lock);
}
