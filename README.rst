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

   west build -b lp_mspm0g3507 . -p always

Flashing
********

After a successful build, flash the board with:

.. code-block:: console

   west flash

Serial Output
*************

The current firmware prints a single startup line:

.. code-block:: console

   Zephyr Audio Analyzer on <board-target>

Development Notes
*****************

The ADC and sensor configuration is already present, but ``src/main.c`` does
not yet read samples or perform audio analysis. The next implementation step is
to replace the startup banner with ADC sampling, signal processing, and the
desired output path.
