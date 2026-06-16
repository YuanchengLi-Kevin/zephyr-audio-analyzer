# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_audio_analyzer)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
  src/main.c
  src/features/audio_analysis/audio_analysis.c
  src/features/audio_input/audio_input.c
  src/features/audio_input_control/audio_input_control.c
  src/features/audio_input/audio_stream_mic/audio_stream_mic.c
  src/features/audio_input/audio_stream_uart/audio_stream_uart.c
  src/features/audio_passthrough/audio_passthrough.c
  src/features/audio_playback/audio_playback.c
  src/features/display_msg/display_msg.c
  src/features/display_startup/display_startup.c
  src/features/display_spectrum/display_spectrum.c
  src/features/display_spectrum/spectrum_scaling/spectrum_scaling.c
)
