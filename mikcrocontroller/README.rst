Microcontroller software
========================

This is a `TrueSTUDIO <https://atollic.com/truestudio/>`_ project with generated
sourcecode by `CubeMX <https://www.st.com/en/development-tools/stm32cubemx.html>`_. Currently
the project is configured for the `STM32F746G-Discovery <https://www.st.com/en/evaluation-tools/32f746gdiscovery.html>`_
discovery board. you should consider checking all include files for
configurations to adapt, when changing to another microcontroller

Structure
---------
All includes are (unfortunately) in the ``Inc`` folder. This is a left-over of
CubeMX's created source code. Some important files:

- ``config.h``: Defines main attributes.
- ``stm32f7xx_hal_conf.h``: Main settings for the HAL. Consider this when
  changing the controller
- ``ads1262.h`` and ``setup.h``: Especially pin and port assignments may change when changing
  the controller
- ``adcp.h``: Defines for the communication protocol


The source code has some more structure:

- ``Src/measure``: All measurement, FFT, ADS1262 and ADC state related things
- ``Src/network``: All protocols (ADCP and WebSocket), network management and
  the HTTP server
- ``Src/system``: Mostly cleaned up CubeMX-code to setup the HAL, peripherals
  and interrupts.

generate_* scripts
------------------
These scripts generate lookup tables for the controller. Each script will create
it's own header file, that declares the data. Note: Do not import these files
more then one times.

The generated files limit the FFT's max bits. When enabling more bits in the
software, new and bigger tables must be created. Check every script for
top-defined variables.
