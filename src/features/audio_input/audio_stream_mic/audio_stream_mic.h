/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_MIC_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_MIC_H_

#include <stdint.h>

int audio_stream_mic_init(void);
int audio_stream_mic_read(uint16_t *sample);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_STREAM_MIC_H_ */
