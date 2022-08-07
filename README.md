# ESP32 Remote Logging

Simple remote logging server. This redirects the output from the logging functions to a server socket that can be connected to from any TCP socket client.

If there is no socket client connected to the ESP32 the logging functions will output to the normal serial output. Once a client is connected the logging output will redirect to the client socket and disable the serial logging.

## Use
- Build and flash the ESP32 project
- Run client.py file 
