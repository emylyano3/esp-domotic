#include "MQTTModule.h"

WiFiClient      _wifiClient;
PubSubClient    _mqttClient(_wifiClient);

void MQTTModule::setServer(const char* host, uint16_t port) {
  _mqttClient.setServer(host, port);
}

void MQTTModule::setCallback( void (*callback)(char* topic, uint8_t* payload, unsigned int length)) {
  _mqttClient.setCallback(callback);
}