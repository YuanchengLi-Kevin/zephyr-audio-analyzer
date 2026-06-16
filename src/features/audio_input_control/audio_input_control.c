/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_input_control/audio_input_control.h"

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "features/audio_input/audio_input.h"
#include "features/display_msg/display_msg.h"

#define DEBOUNCE_DELAY K_MSEC(30)

#define HAS_TOGGLE_BUTTON DT_NODE_EXISTS(DT_ALIAS(audio_input_toggle_button))
#define HAS_MIC_BUTTON DT_NODE_EXISTS(DT_ALIAS(audio_input_mic_button))
#define HAS_UART_BUTTON DT_NODE_EXISTS(DT_ALIAS(audio_input_uart_button))

#if HAS_TOGGLE_BUTTON
static const struct gpio_dt_spec toggle_button =
	GPIO_DT_SPEC_GET(DT_ALIAS(audio_input_toggle_button), gpios);
static struct gpio_callback toggle_button_cb;
static struct k_work_delayable toggle_button_work;
#endif

#if HAS_MIC_BUTTON
static const struct gpio_dt_spec mic_button =
	GPIO_DT_SPEC_GET(DT_ALIAS(audio_input_mic_button), gpios);
static struct gpio_callback mic_button_cb;
static struct k_work_delayable mic_button_work;
#endif

#if HAS_UART_BUTTON
static const struct gpio_dt_spec uart_button =
	GPIO_DT_SPEC_GET(DT_ALIAS(audio_input_uart_button), gpios);
static struct gpio_callback uart_button_cb;
static struct k_work_delayable uart_button_work;
#endif

static void set_input_mode(enum audio_input_mode mode)
{
	int ret = audio_input_set_mode(mode);

	if (ret != 0) {
		printk("Audio input mode switch failed: %d\n", ret);
		return;
	}

	printk("Audio input: %s\n", mode == AUDIO_INPUT_MODE_UART ? "uart" : "mic");
	display_msg_show_input_mode(mode);
}

#if HAS_TOGGLE_BUTTON
static void toggle_button_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	enum audio_input_mode mode = audio_input_get_mode();

	set_input_mode(mode == AUDIO_INPUT_MODE_MIC ? AUDIO_INPUT_MODE_UART : AUDIO_INPUT_MODE_MIC);
}

static void toggle_button_handler(const struct device *port, struct gpio_callback *cb,
				  uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_work_reschedule(&toggle_button_work, DEBOUNCE_DELAY);
}
#endif

#if HAS_MIC_BUTTON
static void mic_button_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	set_input_mode(AUDIO_INPUT_MODE_MIC);
}

static void mic_button_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_work_reschedule(&mic_button_work, DEBOUNCE_DELAY);
}
#endif

#if HAS_UART_BUTTON
static void uart_button_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	set_input_mode(AUDIO_INPUT_MODE_UART);
}

static void uart_button_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_work_reschedule(&uart_button_work, DEBOUNCE_DELAY);
}
#endif

static int configure_button(const struct gpio_dt_spec *button, struct gpio_callback *callback,
			    gpio_callback_handler_t handler)
{
	int ret;

	if (!gpio_is_ready_dt(button)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(button, GPIO_INPUT);
	if (ret != 0) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		return ret;
	}

	gpio_init_callback(callback, handler, BIT(button->pin));
	return gpio_add_callback(button->port, callback);
}

int audio_input_control_init(void)
{
	int ret;

#if HAS_TOGGLE_BUTTON
	k_work_init_delayable(&toggle_button_work, toggle_button_work_handler);
	ret = configure_button(&toggle_button, &toggle_button_cb, toggle_button_handler);
	if (ret != 0) {
		return ret;
	}
#endif

#if HAS_MIC_BUTTON
	k_work_init_delayable(&mic_button_work, mic_button_work_handler);
	ret = configure_button(&mic_button, &mic_button_cb, mic_button_handler);
	if (ret != 0) {
		return ret;
	}
#endif

#if HAS_UART_BUTTON
	k_work_init_delayable(&uart_button_work, uart_button_work_handler);
	ret = configure_button(&uart_button, &uart_button_cb, uart_button_handler);
	if (ret != 0) {
		return ret;
	}
#endif

#if !HAS_TOGGLE_BUTTON && !HAS_MIC_BUTTON && !HAS_UART_BUTTON
	ARG_UNUSED(ret);
	printk("Audio input control: no buttons configured\n");
#endif

	return 0;
}
