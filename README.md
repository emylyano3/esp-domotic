# Generic ESP lib for domotic projects [![Build Status](https://travis-ci.org/emylyano3/esp-domotic.svg?branch=master)](https://travis-ci.org/emylyano3/esp-domotic)

This lib gives an abstraction to other ESP libs going out there that (in my personal experiencie) are repeatedly used over different projects related to home automation (domotics).

It acts like a generic entry point giving simple access to some key functionalities:
- HTTP updates (OTA updates)
- Wifi configuration
- MQTT configuration (configuration is persisted in FS)
- MQTT communication
- MQTT broker reconnection
- LED feedback

To compile project in PlatformIO CLI:

> pio ci .\examples\* --project-conf .\project-conf\platformio.ini --lib=.
