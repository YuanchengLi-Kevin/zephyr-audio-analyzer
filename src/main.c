/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "features/audio_analysis/audio_analysis.h"
#include "features/audio_input/audio_input.h"
#include "features/audio_input_control/audio_input_control.h"
#include "features/audio_passthrough/audio_passthrough.h"
#include "features/audio_playback/audio_playback.h"
#include "features/display_msg/display_msg.h"
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

	ret = audio_input_init();
	if (ret != 0)
	{
		printk("Audio input init failed: %d\n", ret);
		return 0;
	}

	ret = audio_input_control_init();
	if (ret != 0)
	{
		printk("Audio input control init failed: %d\n", ret);
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

	ret = display_msg_init();
	if (ret != 0)
	{
		printk("Display message init failed: %d\n", ret);
		return 0;
	}

	display_msg_show_input_mode(audio_input_get_mode());

	audio_analysis_start();
	audio_passthrough_start();

	return 0;
}
