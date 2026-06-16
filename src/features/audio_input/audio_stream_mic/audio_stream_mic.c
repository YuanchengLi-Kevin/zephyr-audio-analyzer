/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_input/audio_stream_mic/audio_stream_mic.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include "features/audio_config.h"

#define ADC_NODE DT_NODELABEL(adc0)
#define MIC_POWER_NODE DT_PATH(zephyr_user)

static const struct device *const adc_dev = DEVICE_DT_GET(ADC_NODE);
static const struct gpio_dt_spec mic_power =
	GPIO_DT_SPEC_GET(MIC_POWER_NODE, mic_power_gpios);

static const struct adc_channel_cfg adc_channel_cfg = {
	.gain = ADC_GAIN_1,
	.reference = ADC_REF_VDD_1,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id = AUDIO_ADC_CHANNEL_ID,
};

int audio_stream_mic_init(void)
{
	int ret;

	if (!device_is_ready(adc_dev)) {
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&mic_power)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&mic_power, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		return ret;
	}

	return adc_channel_setup(adc_dev, &adc_channel_cfg);
}

int audio_stream_mic_read(uint16_t *sample)
{
	struct adc_sequence sequence = {
		.channels = BIT(AUDIO_ADC_CHANNEL_ID),
		.buffer = sample,
		.buffer_size = sizeof(*sample),
		.resolution = AUDIO_ADC_RESOLUTION,
	};

	return adc_read(adc_dev, &sequence);
}
