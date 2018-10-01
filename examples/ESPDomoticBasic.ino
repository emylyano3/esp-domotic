#include <ESPDomotic.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

ESPDomotic*     _domoticModule;
bool            _condition;

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    Serial.begin(115200);
    _domoticModule->setDebugOutput(DEBUG);
    _domoticModule->setModuleType("testModule");
    _domoticModule->setFeedbackPin(FEEDBACK_PIN);
    _domoticModule->setMqttConnectionCallback(mqttSubscription);
    _domoticModule->setMqttMessageCallback(receiveMqttMessage);
    _domoticModule->init();
    Serial.printf("Station name is: %s", _domoticModule->getModuleName());
}

void loop () {
    _domoticModule->loop();
    if (_condition) {
        Serial.printf("Module name is: %s", _domoticModule->getModuleName());
        Serial.printf("Module location is: %s", _domoticModule->getModuleLocation());
        _domoticModule->getMqttClient()->unsubscribe("oldTopic");
        _domoticModule->getMqttClient()->subscribe("newTopic");
    }
}

void mqttSubscription() {
    _domoticModule->getMqttClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}