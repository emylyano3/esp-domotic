#include <MQTTModule.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

MQTTModule* _mqttModule;
bool        _condition;

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    Serial.begin(115200);
    _mqttModule->setDebugOutput(DEBUG);
    _mqttModule->setModuleType("testModule");
    _mqttModule->setFeedbackPin(FEEDBACK_PIN);
    _mqttModule->setMqttConnectionCallback(mqttSubscription);
    _mqttModule->setMqttMessageCallback(receiveMqttMessage);
    _mqttModule->init();
    Serial.printf("Station name is: %s", _mqttModule->getModuleName());
}

void loop () {
    _mqttModule->loop();
    if (_condition) {
        Serial.printf("Module name is: %s", _mqttModule->getModuleName());
        Serial.printf("Module location is: %s", _mqttModule->getModuleLocation());
        _mqttModule->getMqttClient()->unsubscribe("oldTopic");
        _mqttModule->getMqttClient()->subscribe("newTopic");
    }
}

void mqttSubscription() {
    _mqttModule->getMqttClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}