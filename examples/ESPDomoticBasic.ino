#include <ESPDomotic.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

ESPDomotic*     _domoticModule;
bool            _condition;
Channel         _channel("A", "channel_A", D1, '0', 60 * 1000, OUTPUT);

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    Serial.begin(115200);
    _domoticModule->setDebugOutput(DEBUG);
    _domoticModule->setModuleType("testModule");
    _domoticModule->setFeedbackPin(FEEDBACK_PIN);
    _domoticModule->setPortalSSID("test-module-ssid");
    _domoticModule->addChannel(&_channel);
    _domoticModule->setMqttConnectionCallback(mqttSubscription);
    _domoticModule->setMqttMessageCallback(receiveMqttMessage);
    _domoticModule->init();
    _domoticModule->saveChannelsSettings();
    Serial.printf("Station name is: %s", _domoticModule->getModuleName());
}

void loop () {
    _domoticModule->loop();
    if (_condition) {
        Serial.printf("Module name is: %s", _domoticModule->getModuleName());
        Serial.printf("Module location is: %s", _domoticModule->getModuleLocation());
        _domoticModule->getMqttClient()->unsubscribe("oldTopic");
        _domoticModule->getMqttClient()->subscribe("newTopic");
        _domoticModule->getChannelsCount();
        _domoticModule->getModuleLocation();
        _domoticModule->getModuleName();
        _domoticModule->getChannel(0);
        _domoticModule->getMqttServerHost();
        _domoticModule->getMqttServerPort();
        _domoticModule->getStationName();
    }
}

void mqttSubscription() {
    _domoticModule->getMqttClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}