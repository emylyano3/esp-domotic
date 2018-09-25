#include <FS.h>
#include <ArduinoJson.h>
#include <MQTTModule.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#define PARAM_LENGTH        16

/* Local settings (these could be exported to user through setters) */
/* Wifi connection */
int             _minimumQuality     = 30;
IPAddress       _static_ip_ap       = IPAddress(10,10,10,10);
IPAddress       _static_ip_gw       = IPAddress(10,10,10,10);
IPAddress       _static_ip_sm       = IPAddress(255,255,255,0);
unsigned long   _connectionTimeout  = 5000;
const char*     _portalSSID         = "ESP-Irrigation";

/* Module config */
const char*     _configFile         = "/config.json";

/* MQTT Broker */
unsigned long   _mqttReconectionWait    = 5000;

/* Delegation */
ESPConfig*              _moduleConfig;
PubSubClient*           _mqttClient;
ESP8266WebServer        _httpServer(80);
ESP8266HTTPUpdateServer _httpUpdater;

/* Config params */
ESPConfigParam          _mqttPort (Text, "mqttPort", "MQTT port", "", 6, "required");            // port range is from 0 to 65535
ESPConfigParam          _mqttHost (Text, "mqttHost", "MQTT host", "", PARAM_LENGTH, "required"); // IP max length is 15 chars
ESPConfigParam          _moduleName (Text, "moduleName", "Module name", "", PARAM_LENGTH, "required");
ESPConfigParam          _moduleLocation (Text, "moduleLocation", "Module location", "", PARAM_LENGTH, "required");

/* Control */
unsigned long           _mqttNextReconnection   = 0;

MQTTModule::MQTTModule() {
    String aux = String("esp-module"); //TODO concat ESP.getChipID()
    int length = STATION_NAME_LENGTH > aux.length() ? aux.length() : STATION_NAME_LENGTH;
    aux.toCharArray(_stationName, length);
}

MQTTModule::~MQTTModule() {

}

void MQTTModule::init() {
    /* Wifi connection */
    _moduleConfig->addParameter(&_moduleLocation);
    _moduleConfig->addParameter(&_moduleName);
    _moduleConfig->addParameter(&_mqttHost);
    _moduleConfig->addParameter(&_mqttPort);
    _moduleConfig->setConnectionTimeout(_connectionTimeout);
    _moduleConfig->setPortalSSID(_portalSSID);
    _moduleConfig->setFeedbackPin(_feedbackPin);
    _moduleConfig->setAPStaticIP(_static_ip_ap, _static_ip_gw, _static_ip_sm);
    _moduleConfig->setMinimumSignalQuality(_minimumQuality);
    // _moduleConfig->setStationNameCallback(getStationName);
    // _moduleConfig->setSaveConfigCallback(saveConfig);
    _moduleConfig->connectWifiNetwork(loadConfig());
    _moduleConfig->blockingFeedback(_feedbackPin, 100, 8);

    /* MQTT Broker */
    debug(F("Configuring MQTT broker"));
    String port = String(_mqttPort.getValue());
    debug(F("MQTT Host"), _mqttHost.getValue());
    debug(F("MQTT Port"), port);
    _mqttClient->setServer(_mqttHost.getValue(), (uint16_t) port.toInt());
    if (_mqttCallback) {
        _mqttClient->setCallback(_mqttCallback);
    }

    /* OTA Update */
    debug(F("Setting OTA update"));
    MDNS.begin(getStationName());
    MDNS.addService("http", "tcp", 80);
    _httpUpdater.setup(&_httpServer);
    _httpServer.begin();
    if (_debug) {
        debug(F("HTTPUpdateServer ready.")); 
        debug("Open http://" + String(getStationName()) + ".local/update");
        debug("Open http://" + WiFi.localIP().toString() + "/update");
    }
}

void MQTTModule::loop() {
    _httpServer.handleClient();
    if (!_mqttClient->connected()) {
        connectBroker();
    }
}

/* Setup methods */
void MQTTModule::setDebugOutput(bool debug) {
    _debug = debug;
}

void MQTTModule::setModuleType (String mt) {
    _moduleType = mt;
}

void MQTTModule::setFeedbackPin (uint8_t pin) {
    _feedbackPin = pin;
}

void MQTTModule::setSubscriptionCallback(void(*callback)(void)) {
    _subscriptionCallback = callback;
}

void MQTTModule::setMqttReceiveCallback(void (*callback)(char*, uint8_t*, unsigned int)) {
    _mqttCallback = callback;
}

/* Getters of delegation */
ESPConfig* MQTTModule::getConfig() {
    return _moduleConfig;
}

PubSubClient* MQTTModule::getMQTTClient() {
    return _mqttClient;
}

/* Primitives */
char* MQTTModule::getStationName () {
  if (strlen(_stationName) <= 0) {
    size_t size = _moduleType.length() + _moduleLocation.getValueLength() + _moduleName.getValueLength() + 3;
    String sn;
    sn.concat(_moduleType);
    sn.concat("_");
    sn.concat(_moduleLocation.getValue()); 
    sn.concat("_");
    sn.concat(_moduleName.getValue());
    sn.toCharArray(_stationName, size);
  } 
  return _stationName;
}

bool MQTTModule::loadConfig () {
  size_t size = getFileSize(_configFile);
  if (size > 0) {
    char buff[size];
    loadFile(_configFile, buff, size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buff);
    if (json.success()) {
      if (_debug) {
        json.printTo(Serial);
        Serial.println();
      }
      for (uint8_t i = 0; i < _moduleConfig->getParamsCount(); ++i) {
        _moduleConfig->getParameter(i)->updateValue(json[_moduleConfig->getParameter(i)->getName()]);
        debug(_moduleConfig->getParameter(i)->getName(), _moduleConfig->getParameter(i)->getValue());
      }
      return true;
    } else {
      debug(F("Failed to load json config"));
    }
  }
  return false;
}

void MQTTModule::connectBroker () {
  if (_mqttNextReconnection <= millis()) {
    _mqttNextReconnection = millis() + _mqttReconectionWait;
    debug(F("Connecting MQTT broker as"), getStationName());
    if (_mqttClient->connect(getStationName())) {
      debug(F("MQTT broker Connected"));
      if (_subscriptionCallback) {
        _subscriptionCallback();
      }
    } else {
      debug(F("Failed. RC:"), _mqttClient->state());
    }
  }
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