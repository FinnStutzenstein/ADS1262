Tools for communicate and control the ADS1262 measurement kit
=============================================================

How to use them
------------------
You need at least python 3.6 to run them. Follow the steps below to
set up a virtual environment, to keep your dependencies locally installed::
 
     $ python3 -m venv .venv
     $ source .venv/bin/activate
     $ pip install -U setuptools pip
     $ pip install -U -r requirements.txt

You have to activate the virtuel environment every time, when starting a new
shell session. To deactivate enter "deactivate".

Main manage tool
----------------
The ``manage.py`` is the main tool to control the ADC. Please adjust the settings
in ``settings.py`` for your usecase. If the manager has established a connection,
you can see the status on the left and the command history on the right. Type
``help`` to get an overview of all available commands. With ``quit`` (or shorter ``q``)
you can exit this application.

Sometimes the automatic status updates are missing, if the ADC is stopped, because
it could not send all the measured data. Execute an ``adc update status`` to verify manually.

New commands
------------
If there are new commands added to ADCP, in most cases you just need to add them to
``protocol.json``. The basis structure is a double-dict with prefix and command byte as
keys. Add the command to the right place and give a value for ``command`` (Note: Do not
use the same command twice or let one command be the prefix of another; The software
will confuse these two). If arguments needs to be send, add them in the right order
to the optional ``args``-list. See other commands for possibilities to request and format
arguments. For commands that just returns a statuscode, nothing more is to do.

If the command sends extra data (like ``print osstats`` returns text to display), you
need to add your command to ``commands.py``. Create a class with the name
``<CommandAsCamelCase>Command`` and inherit from ``BaseCommand`` (or some other class
with more precise functionality like ``RemoteCommand``). There are small classes with
custom code: ``MeasurementCreateCommand`` and ``AdcGetStatusCommand`` are good classes
to look for.

Data aggregating tools
----------------------
With ``recieve_data.py`` and ``recieve_fft.py`` you can visualize the measured data.
The same host and port as in ``manage.py`` is used. ``collect_data.py`` writes a
specified amount (or unlimited amount) of samples in a file. Using ``histogram.py``
you can create a live historgram of the measurement.

Calibration
-----------
You can calibrate the ADC with the ``calibrate.py`` script. It will ask some
questions about the current configuration and will guide you throgh the
calibration process.

Debugging
---------
Use ``debug_client.py`` to retrieve debugging output. In the server software use
``printf`` to write to this debugging channel.

Use ``many_recieve_data.py`` and ``many_recieve_fft.py`` to build multiple data- or
fft-connections to the server. Specify the amount as the first parameter. This is
usefull for testing the capabilities of the server.

``make_request.py`` is used to create a simple get request. This is neat for
debugging the HTTP-module.

