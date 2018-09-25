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
    // mqttModule.setModuleType("test");
    // mqttModule.setFeedbackPin(FEEDBACK_PIN);
    // mqttModule.setClientCallback(receiveMessage);
    Serial.begin(115200);
    mqttModule.setDebugOutput(DEBUG);
    mqttModule.setFeedbackPin(FEEDBACK_PIN);
    mqttModule.setSubscriptionCallback(mqttSubscription);
    mqttModule.setModuleType("testModule");
    Serial.printf("Station name is: %s", mqttModule.getStationName());
    mqttModule.init();
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