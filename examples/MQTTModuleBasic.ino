#include <MQTTModule.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

MQTTModule* _mqttModule;
bool        _condition;

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    // mqttModule.setConfigFile("/config.json");
    // mqttModule.setConnectionTimeout(5000);
    // mqttModule.setMinimumSignalQuality(30);
    Serial.begin(115200);
    _mqttModule->setDebugOutput(DEBUG);
    _mqttModule->setModuleType("testModule");
    _mqttModule->setFeedbackPin(FEEDBACK_PIN);
    _mqttModule->setSubscriptionCallback(mqttSubscription);
    _mqttModule->setMqttReceiveCallback(receiveMqttMessage);
    _mqttModule->init();
    Serial.printf("Station name is: %s", _mqttModule->getStationName());
    Serial.printf("Total parameters set: %d", _mqttModule->getConfig()->getParamsCount());
}

void loop () {
    _mqttModule->loop();
    if (_condition) {
        _mqttModule->getMQTTClient()->unsubscribe("oldTopic");
        _mqttModule->getMQTTClient()->subscribe("newTopic");
    }
}

void mqttSubscription() {
    _mqttModule->getMQTTClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}