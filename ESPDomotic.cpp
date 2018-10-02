#include <FS.h>
#include <ESPDomotic.h>
#include <ESPConfig.h>

#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include <ArduinoJson.h>

#ifndef PARAM_LENGTH
#define PARAM_LENGTH 16
#endif

/* Config params */
ESPConfigParam            _mqttPort (Text, "mqttPort", "MQTT port", "", 6, "required");            // port range is from 0 to 65535
ESPConfigParam            _mqttHost (Text, "mqttHost", "MQTT host", "", PARAM_LENGTH, "required"); // IP max length is 15 chars
ESPConfigParam            _moduleName (Text, "moduleName", "Module name", "", PARAM_LENGTH, "required");
ESPConfigParam            _moduleLocation (Text, "moduleLocation", "Module location", "", PARAM_LENGTH, "required");

/* HTTP Update */
ESP8266WebServer          _httpServer(80);
ESP8266HTTPUpdateServer   _httpUpdater;

/* MQTT client */
WiFiClient                _wifiClient;
PubSubClient              _mqttClient(_wifiClient);
/* MQTT broker reconnection control */
unsigned long             _mqttNextConnAtte     = 0;

char                      _stationName[PARAM_LENGTH * 3 + 4];

ESPDomotic::ESPDomotic() {
}

ESPDomotic::~ESPDomotic() {

}

void ESPDomotic::init() {
  debug("MQTT Module INIT");
  /* Wifi connection */
  ESPConfig _moduleConfig;
  _moduleConfig.addParameter(&_moduleLocation);
  _moduleConfig.addParameter(&_moduleName);
  _moduleConfig.addParameter(&_mqttHost);
  _moduleConfig.addParameter(&_mqttPort);
  _moduleConfig.setConnectionTimeout(WIFI_CONNECT_TIMEOUT);
  if (_apSSID) {
    _moduleConfig.setPortalSSID(_apSSID);
  } else {
    String ssid = "Proeza domotic " + String(ESP.getChipId());
    _moduleConfig.setPortalSSID(ssid.c_str());
  }
  _moduleConfig.setMinimumSignalQuality(MIN_SIGNAL_QUALITY);
  _moduleConfig.setStationNameCallback(std::bind(&ESPDomotic::getStationName, this));
  _moduleConfig.setSaveConfigCallback(std::bind(&ESPDomotic::saveConfig, this));
  if (_feedbackPin != INVALID_PIN_NO) {
    _moduleConfig.setFeedbackPin(_feedbackPin);
  }
  _moduleConfig.connectWifiNetwork(loadConfig());
  if (_feedbackPin != INVALID_PIN_NO) {
    _moduleConfig.blockingFeedback(_feedbackPin, 100, 8);
  }
  debug("Connected to wifi....");
  // MQTT Server config
  debug(F("Configuring MQTT broker"));
  debug(F("Port"), getMqttServerPort());
  debug(F("Server"), getMqttServerHost());
  _mqttClient.setServer(getMqttServerHost(), getMqttServerPort());
  if (_mqttMessageCallback) {
    _mqttClient.setCallback(_mqttMessageCallback);
  }
  // OTA Update
  debug(F("Setting OTA update"));
  MDNS.begin(getStationName());
  MDNS.addService("http", "tcp", 80);
  _httpUpdater.setup(&_httpServer);
  _httpServer.begin();
  debug(F("HTTPUpdateServer ready.")); 
  debug("Open http://" + String(getStationName()) + ".local/update");
  debug("Open http://" + WiFi.localIP().toString() + "/update");
}

void ESPDomotic::loop() {
  _httpServer.handleClient();
  if (!_mqttClient.connected()) {
    connectBroker();
  }
  _mqttClient.loop();
}

void ESPDomotic::setDebugOutput(bool debug) {
  _debug = debug;
}

void ESPDomotic::setMqttConnectionCallback(std::function<void()> callback) {
    _mqttConnectionCallback = callback;
}

void ESPDomotic::setMqttMessageCallback(std::function<void(char*, uint8_t*, unsigned int)> callback) {
  _mqttMessageCallback = callback;
}

void ESPDomotic::setFeedbackPin(uint8_t fp) {
  _feedbackPin = fp;
}

void ESPDomotic::setModuleType(const char* type) {
  _moduleType = type;
}

void ESPDomotic::setPortalSSID (const char* ssid) {
  _apSSID = ssid;
}

uint16_t ESPDomotic::getMqttServerPort() {
  return (uint16_t) String(_mqttPort.getValue()).toInt();
}

const char* ESPDomotic::getMqttServerHost() {
  return _mqttHost.getValue();
}

const char* ESPDomotic::getModuleName() {
  return _moduleName.getValue();
}

const char* ESPDomotic::getModuleLocation() {
  return _moduleLocation.getValue();
}

const char* ESPDomotic::getStationName () {
  if (strlen(_stationName) <= 0) {
    size_t size = strlen(_moduleType) + strlen(getModuleLocation()) + strlen(getModuleName()) + 4;
    String sn;
    sn.concat(_moduleType);
    sn.concat("_");
    sn.concat(getModuleLocation()); 
    sn.concat("_");
    sn.concat(getModuleName());
    sn.toCharArray(_stationName, size);
  } 
  return _stationName;
}

PubSubClient* ESPDomotic::getMqttClient() {
  return &_mqttClient;
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
size_t ESPDomotic::getFileSize (const char* fileName) {
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

void ESPDomotic::loadFile (const char* fileName, char buff[], size_t size) {
  File file = SPIFFS.open(fileName, "r");
  file.readBytes(buff, size);
  file.close();
}

void ESPDomotic::connectBroker() {
  if (_mqttNextConnAtte <= millis()) {
    _mqttNextConnAtte = millis() + MQTT_BROKER_CONNECT_RETRY;
    debug(F("Connecting MQTT broker as"), getStationName());
    if (_mqttClient.connect(getStationName())) {
      debug(F("MQTT broker Connected"));
      if (_mqttConnectionCallback) {
        _mqttConnectionCallback();
      }
    } else {
      debug(F("Failed. RC:"), _mqttClient.state());
    }
  }
}

bool ESPDomotic::loadConfig () {
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
void ESPDomotic::saveConfig () {
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

template <class T> void ESPDomotic::debug (T text) {
  if (_debug) {
    Serial.print("*MM: ");
    Serial.println(text);
  }
}

template <class T, class U> void ESPDomotic::debug (T key, U value) {
  if (_debug) {
    Serial.print("*MM: ");
    Serial.print(key);
    Serial.print(": ");
    Serial.println(value);
  }
}