#include <MQTTModule.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

MQTTModule  mqttModule;
bool        _condition;

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    // mqttModule.setConfigFile("/config.json");
    // mqttModule.setConnectionTimeout(5000);
    // mqttModule.setMinimumSignalQuality(30);
    Serial.begin(115200);
    mqttModule.setDebugOutput(DEBUG);
    mqttModule.setFeedbackPin(FEEDBACK_PIN);
    mqttModule.setSubscriptionCallback(mqttSubscription);
    mqttModule.setModuleType("testModule");
    mqttModule.setMqttReceiveCallback(receiveMqttMessage);
    mqttModule.init();
    Serial.printf("Station name is: %s", mqttModule.getStationName());
}

void loop () {
    mqttModule.loop();
    if (_condition) {
        mqttModule.getMQTTClient()->unsubscribe("oldTopic");
        mqttModule.getMQTTClient()->subscribe("newTopic");
    }
}

void mqttSubscription() {
    mqttModule.getMQTTClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}