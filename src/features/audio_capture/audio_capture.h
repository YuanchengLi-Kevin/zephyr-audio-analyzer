/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_CAPTURE_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_CAPTURE_H_

#include <stdint.h>

int audio_capture_init(void);
int audio_capture_read(uint16_t *sample);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_CAPTURE_H_ */
