#include <FS.h>
#include <MQTTModule.h>
#include <ESPConfig.h>
#include <ArduinoJson.h>

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
    _moduleConfig.connectWifiNetwork(loadConfig());
    // if (_feedbackPin != INVALID_PIN_NO) {
    //   _moduleConfig.blockingFeedback(_feedbackPin, 100, 8);
    // }
    Serial.println("Connected to wifi....");
}

void MQTTModule::loop() {
  Serial.println("MQTT Module loop");
}

uint16_t MQTTModule::getMqttServerPort() {
  return (uint16_t) String(_mqttPort.getValue()).toInt();
}

const char* MQTTModule::getMqttServerHost() {
  return _mqttHost.getValue();
}

const char* MQTTModule::getModuleName() {
  return _moduleName.getValue();
}

const char* MQTTModule::getModuleLocation() {
  return _moduleLocation.getValue();
}

/*
  Returns the size of a file. 
  If 
    > the file does not exist
    > the FS cannot be mounted
    > the file cannot be opened for writing
    > the file is empty
  the value returned is 0.
  Otherwise the size of the file is returned.
*/
size_t MQTTModule::getFileSize (const char* fileName) {
  if (SPIFFS.begin()) {
    if (SPIFFS.exists(fileName)) {
      File file = SPIFFS.open(fileName, "r");
      if (file) {
        size_t s = file.size();
        file.close();
        return s;
      } else {
        file.close();
        debug(F("Cant open file"), fileName);
      }
    } else {
      debug(F("File not found"), fileName);
    }
  } else {
    debug(F("Failed to mount FS"));
  }
  return 0;
}

void MQTTModule::loadFile (const char* fileName, char buff[], size_t size) {
  File file = SPIFFS.open(fileName, "r");
  file.readBytes(buff, size);
  file.close();
}

bool MQTTModule::loadConfig () {
  size_t size = getFileSize("/config.json");
  if (size > 0) {
    char buff[size];
    loadFile("/config.json", buff, size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buff);
    if (json.success()) {
      // for (uint8_t i = 0; i < PARAMS_COUNT; ++i) {
      //   _moduleConfig.getParameter(i)->updateValue(json[_moduleConfig.getParameter(i)->getName()]);
      //   debug(_moduleConfig.getParameter(i)->getName(), _moduleConfig.getParameter(i)->getValue());
      // }
      _mqttHost.updateValue(json[_mqttHost.getName()]);
      _mqttPort.updateValue(json[_mqttPort.getName()]);
      _moduleLocation.updateValue(json[_moduleLocation.getName()]);
      _moduleName.updateValue(json[_moduleName.getName()]);
      if (_debug) {
        json.printTo(Serial);
        Serial.println();
      }
      return true;
    } else {
      debug(F("Failed to load json config"));
    }
  }
  return false;
}

/** callback notifying the need to save config */
void MQTTModule::saveConfig () {
  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //TODO Trim param values
    // for (uint8_t i = 0; i < PARAMS_COUNT; ++i) {
    //   json[_moduleConfig.getParameter(i)->getName()] = _moduleConfig.getParameter(i)->getValue();
    // }
    json[_mqttHost.getName()] = _mqttHost.getValue();
    json[_mqttPort.getName()] = _mqttPort.getValue();
    json[_moduleLocation.getName()] = _moduleLocation.getValue();
    json[_moduleName.getName()] = _moduleName.getValue();
    json.printTo(file);
    debug(F("Configuration file saved"));
    if (_debug) {
      json.printTo(Serial);
      Serial.println();
    }
    file.close();
  } else {
    debug(F("Failed to open config file for writing"));
  }
}

char name[15];

/* Primitives */
char* MQTTModule::getStationName () {
  String("ESP-generic").toCharArray(name, 15);
  return name;
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