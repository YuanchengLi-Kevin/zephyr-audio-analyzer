Zephyr Audio Analyzer
#####################

Zephyr Audio Analyzer is a Zephyr RTOS application scaffold for building an
ADC-backed audio or analog signal analyzer.

Building
********

Build for the MSPM0G3507 LaunchPad:

.. code-block:: console

   west build -b lp_mspm0g3507 .

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

The firmware starts an ADC-to-DAC passthrough thread and a lower-priority FFT
thread:

* The passthrough thread samples ``adc0`` channel 5 and immediately writes the
  12-bit value to ``dac0`` channel 0.
* The FFT thread receives copied 256-sample blocks and reports the dominant
  frequency bin periodically without blocking the passthrough path.

On startup, the console prints:

.. code-block:: console

   Zephyr Audio Analyzer ADC->DAC passthrough with FFT on <board-target>

Development Notes
*****************

The first implementation intentionally keeps ADC read and DAC write adjacent in
the passthrough thread. The FFT path receives copied blocks so analysis work
does not add queue latency before the DAC update.
