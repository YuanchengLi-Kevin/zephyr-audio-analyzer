/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_SPECTRUM_SCALING_H_
#define ZEPHYR_AUDIO_ANALYZER_SPECTRUM_SCALING_H_

#include <stdint.h>

#include "features/audio_config.h"
#include "features/display_spectrum/display_spectrum.h"

void spectrum_scaling_fft_to_bars(const int32_t real[AUDIO_FFT_SIZE],
				  const int32_t imag[AUDIO_FFT_SIZE],
				  uint16_t bars[DISPLAY_SPECTRUM_BARS],
				  uint16_t max_height);

#endif /* ZEPHYR_AUDIO_ANALYZER_SPECTRUM_SCALING_H_ */
