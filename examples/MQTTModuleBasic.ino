#include <MQTTModule.h>

MQTTModule mqttModule;

void setup() {
    mqttModule.setCallback(receiveMessage);
    mqttModule.setServer("192.168.0.1", 1883);
}

void loop () {

}

void receiveMessage(char* topic, uint8_t* payload, unsigned int length) {

}