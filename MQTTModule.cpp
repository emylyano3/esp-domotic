#include <MQTTModule.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#define PARAM_LENGTH        16

/* Local settings (these could be exporter to user) */
int             _minimumQuality     = 30;
IPAddress       _static_ip_ap       = IPAddress(10,10,10,10);
IPAddress       _static_ip_gw       = IPAddress(10,10,10,10);
IPAddress       _static_ip_sm       = IPAddress(255,255,255,0);
unsigned long   _connectionTimeout  = 5000;
const char*     _portalSSID         = "ESP-Irrigation";
const char*     _configFile         = "/config.json";

/* Delegation */
ESPConfig*                  _moduleConfig;
PubSubClient*               _mqttClient;
ESP8266WebServer            _httpServer(80);
ESP8266HTTPUpdateServer     _httpUpdater;

/* Config params */
ESPConfigParam          _mqttPort (Text, "mqttPort", "MQTT port", "", 6, "required");            // port range is from 0 to 65535
ESPConfigParam          _mqttHost (Text, "mqttHost", "MQTT host", "", PARAM_LENGTH, "required"); // IP max length is 15 chars
ESPConfigParam          _moduleName (Text, "moduleName", "Module name", "", PARAM_LENGTH, "required");
ESPConfigParam          _moduleLocation (Text, "moduleLocation", "Module location", "", PARAM_LENGTH, "required");

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
    // _moduleConfig->connectWifiNetwork(loadConfig());
    _moduleConfig->blockingFeedback(_feedbackPin, 100, 8);

    /* MQTT Broker */
    debug(F("Configuring MQTT broker"));
    String port = String(_mqttPort.getValue());
    debug(F("MQTT Host"), _mqttHost.getValue());
    debug(F("MQTT Port"), port);
    _mqttClient->setServer(_mqttHost.getValue(), (uint16_t) port.toInt());
    // _mqttClient.setCallback(receiveMqttMessage);

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