ADS1262 management interface
============================

This webpage is based on Angular 7.

Install
-------

Run ``npm install`` to install all dependencies.

Development server
------------------

Start the dev server with ``npm run start``. The page is accessible via
``htto://localhost:4200``. You do not need to build the webpage and put onto the
server, if you want to debug. The websocket connection is not secured by the
Same-Origin-Policy, so just enter the IP of the server.

Deployment
----------

Run ``npm run build`` to create all files in the ``dist/`` folder. Copy this to
the SD card of the server.
