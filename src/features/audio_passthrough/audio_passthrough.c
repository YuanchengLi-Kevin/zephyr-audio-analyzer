/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_passthrough/audio_passthrough.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "features/audio_analysis/audio_analysis.h"
#include "features/audio_capture/audio_capture.h"
#include "features/audio_config.h"
#include "features/audio_playback/audio_playback.h"

#define PASSTHROUGH_STACK_SIZE 2048
#define PASSTHROUGH_PRIORITY 1

K_THREAD_STACK_DEFINE(passthrough_stack, PASSTHROUGH_STACK_SIZE);

static struct k_thread passthrough_thread_data;

static void passthrough_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	while (true) {
		uint16_t sample;
		int ret = audio_capture_read(&sample);

		if (ret == 0) {
			ret = audio_playback_write(sample);
			if (ret == 0) {
				audio_analysis_submit_sample(sample);
			}
		}

		if (ret != 0) {
			printk("Passthrough error: %d\n", ret);
			k_sleep(K_MSEC(100));
		} else {
			k_usleep(AUDIO_SAMPLE_PERIOD_US);
		}
	}
}

void audio_passthrough_start(void)
{
	k_thread_create(&passthrough_thread_data, passthrough_stack,
			K_THREAD_STACK_SIZEOF(passthrough_stack), passthrough_thread,
			NULL, NULL, NULL, PASSTHROUGH_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&passthrough_thread_data, "adc_dac_pass");
}
