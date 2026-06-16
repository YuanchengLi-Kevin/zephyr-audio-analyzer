/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_DISPLAY_SPECTRUM_H_
#define ZEPHYR_AUDIO_ANALYZER_DISPLAY_SPECTRUM_H_

#include <stdint.h>

#include "features/audio_config.h"

#define DISPLAY_SPECTRUM_BARS 32U

int display_spectrum_start(void);
void display_spectrum_submit_fft(const int32_t real[AUDIO_FFT_SIZE],
				 const int32_t imag[AUDIO_FFT_SIZE]);

#endif /* ZEPHYR_AUDIO_ANALYZER_DISPLAY_SPECTRUM_H_ */
