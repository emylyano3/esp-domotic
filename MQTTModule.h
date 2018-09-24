#ifndef MQTTModule_h
#define MQTTModule_h

#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>

class MQTTModule {

    public:
        void    setServer(const char* host, uint16_t port);
        void    setCallback(void (*callback)(char*, uint8_t*, unsigned int));

    private:
};

#endif