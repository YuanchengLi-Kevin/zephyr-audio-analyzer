/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_playback/audio_playback.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/gpio.h>

#include "features/audio_config.h"

#define DAC_NODE DT_NODELABEL(dac0)
#define AMP_CONTROL_NODE DT_PATH(zephyr_user)

static const struct device *const dac_dev = DEVICE_DT_GET(DAC_NODE);
static const struct gpio_dt_spec amp_control =
	GPIO_DT_SPEC_GET(AMP_CONTROL_NODE, amp_control_gpios);

static const struct dac_channel_cfg dac_channel_cfg = {
	.channel_id = AUDIO_DAC_CHANNEL_ID,
	.resolution = AUDIO_DAC_RESOLUTION,
	.buffered = true,
	.internal = true,
};

int audio_playback_init(void)
{
	int ret;

	if (!device_is_ready(dac_dev))
	{
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&amp_control))
	{
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&amp_control, GPIO_OUTPUT_INACTIVE);
	if (ret != 0)
	{
		return ret;
	}

	ret = dac_channel_setup(dac_dev, &dac_channel_cfg);
	if (ret != 0)
	{
		return ret;
	}

	return gpio_pin_set_dt(&amp_control, 0);
}

int audio_playback_write(uint16_t sample)
{
	return dac_write_value(dac_dev, AUDIO_DAC_CHANNEL_ID, sample);
}
