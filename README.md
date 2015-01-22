# Wifi service implementation for ESP8266
Implements an asynchronous background service for the ESP8266 wifi module.
The version of the module that have been tested is 0.91 integrated with PIC32.

At the moment the program space used is not optimized. I will add some updates when I finish the module.

## Features
 * Background async response to service using buffered input
 * Supports multiple modules on hardware, by using one service per module.
 * Can do parallel tcp calls
 * Uses callbacks for async tcp responses

## Todo list
 * Integrate server capabilities and route requests
 * Better failure management on each service states using callbacks

## Example
Follow the instructions on https://github.com/phaetto/PIC32-ESP8266-WifiExample

The software is provided under The MIT License.
