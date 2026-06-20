.. Copyright (c) 2026 Yuancheng Li
.. SPDX-License-Identifier: Apache-2.0

Zephyr Audio Analyzer
#####################

Zephyr Audio Analyzer is a Zephyr RTOS application scaffold for building an
ADC-backed audio or analog signal analyzer.

Building
********

Build for the MSPM0G3507 LaunchPad:

To force a clean configure:

.. code-block:: console

   west build -b lp_mspm0g3507 -p

Flashing
********

After a successful build, flash the board with:

.. code-block:: console

   west flash -d build -r openocd -- --openocd "<path-to-openocd-ti>/src/openocd.exe" --openocd-search "<path-to-openocd-ti>/tcl"

Runtime Behavior
****************

The firmware initializes an ILI9341 SPI LCD, then starts an ADC-to-DAC
passthrough thread and a lower-priority FFT thread:

* The passthrough thread samples ``adc0`` channel 5 and immediately writes the
  12-bit value to ``dac0`` channel 0.
* The FFT thread receives copied 256-sample blocks and reports the dominant
  frequency bin periodically without blocking the passthrough path.

On startup, the console prints:

.. code-block:: console

   Zephyr Audio Analyzer ADC->DAC passthrough with FFT and LCD on <board-target>


UART Audio Input
****************

The ESP32-C6 sends unsigned 12-bit PCM audio to the MSPM0G3507 over a
receive-only UART connection. The board overlay selects ``uart1`` as
``zephyr,audio-uart`` and configures PA9 as UART1 RX.

UART settings and wiring:

* Baud rate: 2,000,000
* Format: 8 data bits, no parity, 1 stop bit (8N1)
* Logic level: 3.3 V
* ESP32-C6 GPIO4 / UART1 TX -> MSPM0G3507 PA9 / UART1 RX
* ESP32-C6 GND -> MSPM0G3507 GND
* No MSPM0 TX connection is currently required.

On the MSPM0G3507 LaunchPad, move J14 to connect PA9 for UART1 RX at the
position shared with SW1. Remove J18 when using the Audio BoosterPack.

UART Frame Format
=================

Each frame contains one to 128 samples:

.. code-block:: text

   byte 0:     0xA5
   byte 1:     0x5A
   byte 2:     sequence number, uint8_t, wraps from 255 to 0
   byte 3:     sample count, uint8_t, 1..128
   bytes 4+:   samples, little-endian uint16_t

Samples use the range 0..4095, with 2048 as the midpoint. The expected sample
rate is 42,000 samples/sec. A frame containing a sample outside the 12-bit
range is discarded.

Receiver Buffering
==================

The UART interrupt handler parses and queues complete frames rather than
queueing every sample separately. The queue holds eight frames, or up to 1024
samples when the sender uses 128-sample frames. The playback path waits for at
least 256 queued samples before starting and applies the same prefill again
after a read timeout.

Only the passthrough thread reads the UART stream. It sends each received
sample to the DAC and copies it to the FFT input, preventing playback and FFT
from stealing samples from each other.

UART Diagnostics
================

The receiver prints cumulative diagnostics once per second:

.. code-block:: text

   UART audio: received_bytes=... valid_frames=... sequence_gaps=... queued_samples=... dropped_uart_samples=... read_timeouts=... invalid_samples=...

``dropped_uart_samples`` increasing means complete frames could not be queued,
usually because the consumer is too slow. ``read_timeouts`` increasing while
``received_bytes`` stops indicates that the ESP32-C6 UART/DMA path has stopped
supplying data. Sequence gaps identify frames missing between the ESP32-C6 and
the receiver. Invalid samples usually indicate corruption or a protocol
mismatch.

ILI9341 LCD Wiring
******************

The LP-MSPM0G3507 overlay uses Zephyr's native ``ilitek,ili9341`` display
driver through the ``zephyr,mipi-dbi-spi`` bridge on ``spi0``.

Default display wiring:

* ``SCK``: ``PB18``
* ``MOSI`` / ``PICO``: ``PB17``
* ``MISO`` / ``POCI``: ``PB19``; optional because the display is configured
  ``write-only``
* ``CS``: ``PA8``
* ``D/C``: ``PB20``
* ``RESET``: ``PB21``, active low
* ``BACKLIGHT``: ``PB27``, active high

Adjust ``boards/lp_mspm0g3507.overlay`` if your LCD module is wired
differently.

Development Notes
*****************

The first implementation intentionally keeps ADC read and DAC write adjacent in
the passthrough thread. The FFT path receives copied blocks so analysis work
does not add queue latency before the DAC update.
