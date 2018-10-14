#include <ESPDomotic.h>

#define DEBUG           true
#define FEEDBACK_PIN    D7

ESPDomotic*     _domoticModule;
bool            _condition;
bool            _reset;
Channel         _channel("A", "channel_A", D1, OUTPUT, HIGH, 60 * 1000);

void setup() {
    // mqttModule.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
    Serial.begin(115200);
    _domoticModule->setModuleType("testModule");
    _domoticModule->setFeedbackPin(FEEDBACK_PIN);
    _domoticModule->setPortalSSID("test-module-ssid");
    _domoticModule->addChannel(&_channel);
    _domoticModule->setMqttConnectionCallback(mqttSubscription);
    _domoticModule->setMqttMessageCallback(receiveMqttMessage);
    _domoticModule->init();
    _domoticModule->saveChannelsSettings();
    int fileSize = _domoticModule->getFileSize("/file.txt");
    char buff[fileSize];
    _domoticModule->loadFile("file.txt", buff, fileSize);
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
        _domoticModule->getStationTopic("cmd");
        _domoticModule->getChannelTopic(_domoticModule->getChannel(0), "cmd");
    }
    if (_reset) {
        _domoticModule->moduleHardReset();
    }
}

void mqttSubscription() {
    _domoticModule->getMqttClient()->subscribe("oldTopic");
}

void receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {

}