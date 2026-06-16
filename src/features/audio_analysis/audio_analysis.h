/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_ANALYSIS_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_ANALYSIS_H_

#include <stdint.h>

void audio_analysis_start(void);
void audio_analysis_submit_sample(uint16_t sample);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_ANALYSIS_H_ */
