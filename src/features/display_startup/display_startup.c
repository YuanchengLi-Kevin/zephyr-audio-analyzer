/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/display_startup/display_startup.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

#define STARTUP_COLOR_BACKGROUND 0x0000
#define STARTUP_COLOR_TEXT 0xffff
#define STARTUP_COLOR_ACCENT 0x07e0
#define STARTUP_TEXT_SCALE 4U
#define STARTUP_GLYPH_WIDTH 5U
#define STARTUP_GLYPH_HEIGHT 7U
#define STARTUP_GLYPH_SPACING 1U
#define STARTUP_SPLASH_MS 2000
#define STARTUP_MAX_LINE_PIXELS 320U

static const struct device *const display_dev = DEVICE_DT_GET(DISPLAY_NODE);
static struct display_capabilities display_caps;
static uint16_t line_buffer[STARTUP_MAX_LINE_PIXELS];

static const uint8_t glyph_space[STARTUP_GLYPH_HEIGHT] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t glyph_a[STARTUP_GLYPH_HEIGHT] = {
	0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11,
};

static const uint8_t glyph_d[STARTUP_GLYPH_HEIGHT] = {
	0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e,
};

static const uint8_t glyph_i[STARTUP_GLYPH_HEIGHT] = {
	0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f,
};

static const uint8_t glyph_o[STARTUP_GLYPH_HEIGHT] = {
	0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e,
};

static const uint8_t glyph_p[STARTUP_GLYPH_HEIGHT] = {
	0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10,
};

static const uint8_t glyph_r[STARTUP_GLYPH_HEIGHT] = {
	0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11,
};

static const uint8_t glyph_s[STARTUP_GLYPH_HEIGHT] = {
	0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e,
};

static const uint8_t glyph_t[STARTUP_GLYPH_HEIGHT] = {
	0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
};

static const uint8_t glyph_u[STARTUP_GLYPH_HEIGHT] = {
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e,
};

static const uint8_t *glyph_for_char(char c)
{
	switch (c) {
	case 'A':
		return glyph_a;
	case 'D':
		return glyph_d;
	case 'I':
		return glyph_i;
	case 'O':
		return glyph_o;
	case 'P':
		return glyph_p;
	case 'R':
		return glyph_r;
	case 'S':
		return glyph_s;
	case 'T':
		return glyph_t;
	case 'U':
		return glyph_u;
	default:
		return glyph_space;
	}
}

static uint16_t text_width(const char *text, uint16_t scale)
{
	size_t len = strlen(text);

	if (len == 0U) {
		return 0U;
	}

	return (uint16_t)(((STARTUP_GLYPH_WIDTH + STARTUP_GLYPH_SPACING) * len -
			   STARTUP_GLYPH_SPACING) *
			  scale);
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

static int clear_display(uint16_t color)
{
	return write_solid_rect(0, 0, display_caps.x_resolution, display_caps.y_resolution, color);
}

static int draw_char(uint16_t x, uint16_t y, char c, uint16_t scale, uint16_t color)
{
	const uint8_t *glyph = glyph_for_char(c);

	for (uint16_t row = 0; row < STARTUP_GLYPH_HEIGHT; row++) {
		for (uint16_t col = 0; col < STARTUP_GLYPH_WIDTH; col++) {
			if ((glyph[row] & BIT(STARTUP_GLYPH_WIDTH - 1U - col)) == 0U) {
				continue;
			}

			int ret = write_solid_rect(x + (col * scale), y + (row * scale),
						   scale, scale, color);

			if (ret != 0) {
				return ret;
			}
		}
	}

	return 0;
}

static int draw_text_centered(uint16_t y, const char *text, uint16_t scale, uint16_t color)
{
	uint16_t width = text_width(text, scale);
	uint16_t x = 0U;

	if (width < display_caps.x_resolution) {
		x = (display_caps.x_resolution - width) / 2U;
	}

	for (size_t i = 0; text[i] != '\0'; i++) {
		int ret = draw_char(x, y, text[i], scale, color);

		if (ret != 0) {
			return ret;
		}

		x += (STARTUP_GLYPH_WIDTH + STARTUP_GLYPH_SPACING) * scale;
	}

	return 0;
}

static int show_startup_splash(void)
{
	uint16_t text_height = STARTUP_GLYPH_HEIGHT * STARTUP_TEXT_SCALE;
	uint16_t top = (display_caps.y_resolution - (text_height * 2U) - 16U) / 2U;
	int ret;

	ret = clear_display(STARTUP_COLOR_BACKGROUND);
	if (ret != 0) {
		return ret;
	}

	ret = draw_text_centered(top, "AUDIO", STARTUP_TEXT_SCALE, STARTUP_COLOR_TEXT);
	if (ret != 0) {
		return ret;
	}

	ret = draw_text_centered(top + text_height + 16U, "STARTUP", STARTUP_TEXT_SCALE,
				 STARTUP_COLOR_ACCENT);
	if (ret != 0) {
		return ret;
	}

	k_sleep(K_MSEC(STARTUP_SPLASH_MS));

	return clear_display(STARTUP_COLOR_BACKGROUND);
}

int display_startup_init(void)
{
	int ret;

	if (!device_is_ready(display_dev)) {
		return -ENODEV;
	}

	display_get_capabilities(display_dev, &display_caps);

	ret = display_blanking_off(display_dev);
	if (ret != 0) {
		return ret;
	}

	ret = show_startup_splash();
	if (ret != 0) {
		return ret;
	}

	printk("ILI9341 display ready: %ux%u\n", display_caps.x_resolution,
	       display_caps.y_resolution);

	return 0;
}

const struct device *display_startup_get_device(void)
{
	return display_dev;
}

void display_startup_get_capabilities(struct display_capabilities *capabilities)
{
	if (capabilities == NULL) {
		return;
	}

	memcpy(capabilities, &display_caps, sizeof(display_caps));
}
