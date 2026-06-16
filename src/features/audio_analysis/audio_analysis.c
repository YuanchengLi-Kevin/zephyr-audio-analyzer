/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "features/audio_analysis/audio_analysis.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "features/audio_config.h"

#define FFT_STACK_SIZE 4096
#define FFT_PRIORITY 5
#define FFT_REPORT_INTERVAL_BLOCKS 8

struct sample_block {
	uint16_t samples[AUDIO_FFT_SIZE];
	uint32_t sequence;
};

struct q15_twiddle {
	int32_t real;
	int32_t imag;
};

K_MSGQ_DEFINE(fft_msgq, sizeof(struct sample_block), 2, 4);
K_THREAD_STACK_DEFINE(fft_stack, FFT_STACK_SIZE);

static struct k_thread fft_thread_data;
static struct sample_block capture_block;
static uint16_t capture_pos;
static uint32_t capture_sequence;
static uint32_t dropped_fft_blocks;

static int32_t fft_real[AUDIO_FFT_SIZE];
static int32_t fft_imag[AUDIO_FFT_SIZE];

static const struct q15_twiddle twiddle_steps[AUDIO_FFT_BITS] = {
	{ -32768, 0 },
	{ 0, -32768 },
	{ 23170, -23170 },
	{ 30274, -12540 },
	{ 32138, -6393 },
	{ 32610, -3212 },
	{ 32729, -1608 },
	{ 32757, -804 },
};

static uint16_t reverse_bits(uint16_t value)
{
	uint16_t reversed = 0;

	for (uint8_t i = 0; i < AUDIO_FFT_BITS; i++) {
		reversed <<= 1;
		reversed |= value & 1U;
		value >>= 1;
	}

	return reversed;
}

static int32_t q15_mul(int32_t a, int32_t b)
{
	return (a * b) >> 15;
}

static uint32_t abs32(int32_t value)
{
	return value < 0 ? (uint32_t)-value : (uint32_t)value;
}

void audio_analysis_submit_sample(uint16_t sample)
{
	capture_block.samples[capture_pos++] = sample;

	if (capture_pos < AUDIO_FFT_SIZE) {
		return;
	}

	capture_block.sequence = capture_sequence++;
	if (k_msgq_put(&fft_msgq, &capture_block, K_NO_WAIT) != 0) {
		dropped_fft_blocks++;
	}

	capture_pos = 0;
}

static void fft_prepare_input(const struct sample_block *block)
{
	for (uint16_t i = 0; i < AUDIO_FFT_SIZE; i++) {
		uint16_t reversed = reverse_bits(i);
		int32_t centered = (int32_t)block->samples[i] - AUDIO_ADC_MIDPOINT;

		fft_real[reversed] = centered << 4;
		fft_imag[reversed] = 0;
	}
}

static void fft_run(void)
{
	for (uint8_t stage = 0; stage < AUDIO_FFT_BITS; stage++) {
		uint16_t half_size = BIT(stage);
		uint16_t block_size = half_size << 1;
		const struct q15_twiddle wm = twiddle_steps[stage];
		int32_t w_real = 32767;
		int32_t w_imag = 0;

		for (uint16_t j = 0; j < half_size; j++) {
			for (uint16_t k = j; k < AUDIO_FFT_SIZE; k += block_size) {
				uint16_t pair = k + half_size;
				int32_t odd_real = q15_mul(w_real, fft_real[pair]) -
						   q15_mul(w_imag, fft_imag[pair]);
				int32_t odd_imag = q15_mul(w_real, fft_imag[pair]) +
						   q15_mul(w_imag, fft_real[pair]);
				int32_t even_real = fft_real[k];
				int32_t even_imag = fft_imag[k];

				fft_real[k] = (even_real + odd_real) >> 1;
				fft_imag[k] = (even_imag + odd_imag) >> 1;
				fft_real[pair] = (even_real - odd_real) >> 1;
				fft_imag[pair] = (even_imag - odd_imag) >> 1;
			}

			int32_t next_real = q15_mul(w_real, wm.real) - q15_mul(w_imag, wm.imag);
			int32_t next_imag = q15_mul(w_real, wm.imag) + q15_mul(w_imag, wm.real);

			w_real = next_real;
			w_imag = next_imag;
		}
	}
}

static uint16_t fft_peak_bin(void)
{
	uint16_t peak_bin = 1;
	uint32_t peak_mag = 0;

	for (uint16_t i = 1; i < AUDIO_FFT_SIZE / 2; i++) {
		uint32_t mag = abs32(fft_real[i]) + abs32(fft_imag[i]);

		if (mag > peak_mag) {
			peak_mag = mag;
			peak_bin = i;
		}
	}

	return peak_bin;
}

static uint16_t block_peak_to_peak(const struct sample_block *block)
{
	uint16_t min_sample = UINT16_MAX;
	uint16_t max_sample = 0;

	for (uint16_t i = 0; i < AUDIO_FFT_SIZE; i++) {
		min_sample = MIN(min_sample, block->samples[i]);
		max_sample = MAX(max_sample, block->samples[i]);
	}

	return max_sample - min_sample;
}

static void fft_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	struct sample_block block;
	uint32_t processed_blocks = 0;

	while (true) {
		k_msgq_get(&fft_msgq, &block, K_FOREVER);

		fft_prepare_input(&block);
		fft_run();

		processed_blocks++;
		if ((processed_blocks % FFT_REPORT_INTERVAL_BLOCKS) == 0) {
			uint16_t peak_bin = fft_peak_bin();
			uint32_t peak_hz = (peak_bin * AUDIO_SAMPLE_RATE_HZ) / AUDIO_FFT_SIZE;

			printk("FFT block %u: peak=%u Hz bin=%u p2p=%u dropped=%u\n",
			       block.sequence, peak_hz, peak_bin, block_peak_to_peak(&block),
			       dropped_fft_blocks);
		}
	}
}

void audio_analysis_start(void)
{
	k_thread_create(&fft_thread_data, fft_stack, K_THREAD_STACK_SIZEOF(fft_stack),
			fft_thread, NULL, NULL, NULL, FFT_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&fft_thread_data, "fft");
}
