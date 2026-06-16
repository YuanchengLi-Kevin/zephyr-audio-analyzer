/*
 * Copyright (c) 2026 Yuancheng Li
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/display_spectrum/spectrum_scaling/spectrum_scaling.h"

#include <stdint.h>

#include <zephyr/sys/util.h>

/*
 * Log-spaced-ish visualizer edges from the first useful FFT bin toward the
 * top of the audible range. Low frequencies get more display space than a
 * linear bin split, which reads better for music.
 */
static const uint16_t band_edges_hz[DISPLAY_SPECTRUM_BARS + 1U] = {
	180,   209,   242,   281,   326,   378,   439,   509,   590,
	684,   794,   921,   1068,  1239,  1438,  1668,  1935,  2245,
	2605,  3022,  3506,  4067,  4718,  5473,  6349,  7365,  8544,
	9911,  11497, 13337, 15471, 17946, 20000,
};

static uint32_t abs32(int32_t value)
{
	return value < 0 ? (uint32_t)-value : (uint32_t)value;
}

static uint16_t hz_to_bin(uint32_t hz)
{
	uint32_t bin = (hz * AUDIO_FFT_SIZE + AUDIO_SAMPLE_RATE_HZ - 1U) /
		       AUDIO_SAMPLE_RATE_HZ;

	return (uint16_t)CLAMP(bin, 1U, (AUDIO_FFT_SIZE / 2U) - 1U);
}

static uint32_t sqrt_u32(uint32_t value)
{
	uint32_t root = 0U;
	uint32_t bit = 1UL << 30;

	while (bit > value) {
		bit >>= 2;
	}

	while (bit != 0U) {
		if (value >= root + bit) {
			value -= root + bit;
			root = (root >> 1) + bit;
		} else {
			root >>= 1;
		}

		bit >>= 2;
	}

	return root;
}

void spectrum_scaling_fft_to_bars(const int32_t real[AUDIO_FFT_SIZE],
				  const int32_t imag[AUDIO_FFT_SIZE],
				  uint16_t bars[DISPLAY_SPECTRUM_BARS],
				  uint16_t max_height)
{
	uint32_t magnitudes[DISPLAY_SPECTRUM_BARS] = { 0 };
	uint32_t peak = 1U;

	for (uint16_t bar = 0; bar < DISPLAY_SPECTRUM_BARS; bar++) {
		uint16_t start_bin = hz_to_bin(band_edges_hz[bar]);
		uint16_t end_bin = hz_to_bin(band_edges_hz[bar + 1U]);
		uint32_t sum = 0U;
		uint16_t count = 0U;

		if (end_bin <= start_bin) {
			end_bin = start_bin + 1U;
		}

		end_bin = MIN(end_bin, (AUDIO_FFT_SIZE / 2U));

		for (uint16_t bin = start_bin; bin < end_bin; bin++) {
			sum += abs32(real[bin]) + abs32(imag[bin]);
			count++;
		}

		magnitudes[bar] = sqrt_u32(count > 0U ? sum / count : 0U);
		peak = MAX(peak, magnitudes[bar]);
	}

	for (uint16_t bar = 0; bar < DISPLAY_SPECTRUM_BARS; bar++) {
		bars[bar] = (uint16_t)((magnitudes[bar] * max_height) / peak);
	}
}
