/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_AUDIO_ANALYZER_AUDIO_PLAYBACK_H_
#define ZEPHYR_AUDIO_ANALYZER_AUDIO_PLAYBACK_H_

#include <stdint.h>

int audio_playback_init(void);
int audio_playback_write(uint16_t sample);

#endif /* ZEPHYR_AUDIO_ANALYZER_AUDIO_PLAYBACK_H_ */
