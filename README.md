# Generic ESP Module for Domotics

This lib gives an abstraction to other ESP libs going out there that (in my personal experiencie) are repeatedly used over different projects related to home automation (domotics).

It acts like a generic entry point giving simple access to some key functionalities:
- HTTP updates (OTA updates)
- Wifi configuration
- MQTT communication
- MQTT configuration (configuration is persisted in FS)
- MQTT broker reconnection
- LED feedback

To compile project in PlatformIO:

> pio ci .\examples\ --project-conf .\project-conf\platformio.ini --lib=.
