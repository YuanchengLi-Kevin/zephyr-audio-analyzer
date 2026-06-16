/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "features/audio_analysis/audio_analysis.h"
#include "features/audio_capture/audio_capture.h"
#include "features/audio_passthrough/audio_passthrough.h"
#include "features/audio_playback/audio_playback.h"
#include "features/display_spectrum/display_spectrum.h"
#include "features/display_startup/display_startup.h"

int main(void)
{
	int ret;
	// Init core features
	ret = display_startup_init();
	if (ret != 0)
	{
		printk("Display init failed: %d\n", ret);
		return 0;
	}

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

	printk("Zephyr Audio Analyzer ADC->DAC passthrough with FFT and LCD on %s\n",
		   CONFIG_BOARD_TARGET);

	// Start threads
	ret = display_spectrum_start();
	if (ret != 0)
	{
		printk("Display spectrum start failed: %d\n", ret);
		return 0;
	}

	audio_analysis_start();
	audio_passthrough_start();

	return 0;
}
