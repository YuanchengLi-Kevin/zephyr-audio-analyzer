/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "features/audio_analysis/audio_analysis.h"
#include "features/audio_capture/audio_capture.h"
#include "features/audio_passthrough/audio_passthrough.h"
#include "features/audio_playback/audio_playback.h"

int main(void)
{
	int ret;

	ret = audio_capture_init();
	if (ret != 0)
	{
		printk("Audio capture init failed: %d\n", ret);
		return 0;
	}

	ret = audio_playback_init();
	if (ret != 0)
	{
		printk("Audio playback init failed: %d\n", ret);
		return 0;
	}

	printf("Zephyr Audio Analyzer ADC->DAC passthrough with FFT on %s\n",
		   CONFIG_BOARD_TARGET);

	audio_analysis_start();
	audio_passthrough_start();

	return 0;
}
