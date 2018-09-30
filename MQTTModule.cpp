#include <MQTTModule.h>
#include <ESPConfig.h>

/* Config params */
ESPConfigParam          _mqttPort (Text, "mqttPort", "MQTT port", "", 6, "required");            // port range is from 0 to 65535
ESPConfigParam          _mqttHost (Text, "mqttHost", "MQTT host", "", PARAM_LENGTH, "required"); // IP max length is 15 chars
ESPConfigParam          _moduleName (Text, "moduleName", "Module name", "", PARAM_LENGTH, "required");
ESPConfigParam          _moduleLocation (Text, "moduleLocation", "Module location", "", PARAM_LENGTH, "required");

MQTTModule::MQTTModule() {
}

MQTTModule::~MQTTModule() {

}

void MQTTModule::init() {
    Serial.println("MQTT Module INIT");
    /* Wifi connection */
    ESPConfig _moduleConfig;
    _moduleConfig.addParameter(&_moduleLocation);
    _moduleConfig.addParameter(&_moduleName);
    _moduleConfig.addParameter(&_mqttHost);
    _moduleConfig.addParameter(&_mqttPort);
    _moduleConfig.setConnectionTimeout(5000);
    _moduleConfig.setPortalSSID("ESP-Irrigation");
    _moduleConfig.setMinimumSignalQuality(30);
    _moduleConfig.setStationNameCallback(std::bind(&MQTTModule::getStationName, this));
    _moduleConfig.setSaveConfigCallback(std::bind(&MQTTModule::saveConfig, this));
    // if (_feedbackPin != INVALID_PIN_NO) {
    //   _moduleConfig.setFeedbackPin(_feedbackPin);
    // }
    _moduleConfig.connectWifiNetwork(true);
    // if (_feedbackPin != INVALID_PIN_NO) {
    //   _moduleConfig.blockingFeedback(_feedbackPin, 100, 8);
    // }
    Serial.println("Connected to wifi....");
}

void MQTTModule::loop() {
  Serial.println("MQTT Module loop");
}

char name[15];

/* Primitives */
char* MQTTModule::getStationName () {
  String("ESP-generic").toCharArray(name, 15);
  return name;
}

void MQTTModule::saveConfig () {
  debug("Saving config");
}

template <class T> void MQTTModule::debug (T text) {
  if (_debug) {
    Serial.print("*MM: ");
    Serial.println(text);
  }
}

template <class T, class U> void MQTTModule::debug (T key, U value) {
  if (_debug) {
    Serial.print("*MM: ");
    Serial.print(key);
    Serial.print(": ");
    Serial.println(value);
  }
}