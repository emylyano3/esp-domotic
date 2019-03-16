#include <ESPDomotic.hpp>
#include "Logger.hpp"

#include <ESP8266HTTPUpdateServer.h>

#ifndef ESP01
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#endif

/* HTTP Update */
ESP8266WebServer          _httpServer(80);
ESP8266HTTPUpdateServer   _httpUpdater;

WiFiClient                _wifiClient;

#ifndef MQTT_OFF
/* MQTT client */
PubSubClient              _mqttClient(_wifiClient);
/* MQTT broker reconnection control */
unsigned long             _mqttNextConnAtte     = 0;
#endif 

char                      _stationName[_paramValueMaxLength * 3 + 4];

bool            _runningStandAlone    = false;
const uint8_t   _channelsMax          = 5;
uint8_t         _channelsCount        = 0;

uint16_t        _wifiConnectTimeout   = 30;
uint16_t        _configPortalTimeout  = 60;
uint16_t        _configFileSize       = 200;

uint8_t         _qos                  = 1;

const char*     _homieVersion         = "3.1.0";
const char*     _implementation       = "esp8266";
const char*     _fwVersion            = NULL;
const char*     _fwName               = NULL;

ESPDomotic::ESPDomotic() {
  _channels = (Channel**)malloc(_channelsMax * sizeof(Channel*));
  #ifndef MQTT_OFF
  _mqttPort = new ESPConfigParam(Text, "mqttPort", "MQTT port", "", _paramPortValueLength, "required");
  _mqttHost = new ESPConfigParam(Text, "mqttHost", "MQTT host", "", _paramIPValueLength, "required");
  #endif
  _moduleName = new ESPConfigParam(Text, "moduleName", "Module name", "", _paramValueMaxLength, "required");
  _moduleLocation = new ESPConfigParam(Text, "moduleLocation", "Module location", "", _paramValueMaxLength, "required");
  _settings = new Settings(_mqttHost, _mqttPort, _moduleName, _moduleLocation, _channels, _channelsCount);
} 

ESPDomotic::~ESPDomotic() {
  #ifndef MQTT_OFF
  delete _mqttPort;
  delete _mqttHost;
  #endif
  delete _moduleName;
  delete _moduleLocation;
  delete _settings;
  //free(_channels);
}

void ESPDomotic::init() {
  #ifdef LOGGING
  debug(F("ESP Domotic module INIT"));
  #endif
  /* Wifi connection */
  ESPConfig _moduleConfig;
  _moduleConfig.addParameter(_moduleLocation);
  _moduleConfig.addParameter(_moduleName);
  #ifndef MQTT_OFF
  _moduleConfig.addParameter(_mqttHost);
  _moduleConfig.addParameter(_mqttPort);
  #endif
  _moduleConfig.setWifiConnectTimeout(_wifiConnectTimeout);
  _moduleConfig.setConfigPortalTimeout(_configPortalTimeout);
  _moduleConfig.setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
  if (_apSSID) {
    _moduleConfig.setPortalSSID(_apSSID);
  } else {
    String ssid = "Proeza domotic " + String(ESP.getChipId());
    _moduleConfig.setPortalSSID(ssid.c_str());
  }
  _moduleConfig.setMinimumSignalQuality(_wifiMinSignalQuality);
  _moduleConfig.setStationNameCallback(std::bind(&ESPDomotic::getStationName, this));
  _moduleConfig.setSaveConfigCallback(std::bind(&ESPDomotic::saveConfig, this));
  if (_feedbackPin != _invalidPinNo) {
    pinMode(_feedbackPin, OUTPUT);
    _moduleConfig.setFeedbackPin(_feedbackPin);
  }
  _runningStandAlone = !_moduleConfig.connectWifiNetwork(_settings->loadConfig());
  #ifdef LOGGING
  debug(F("Connected to wifi"), _runningStandAlone ? "false" : "true");
  #endif
  if (_feedbackPin != _invalidPinNo) {
    if (_runningStandAlone) {
      // could not connect to a wifi net
      _moduleConfig.blockingFeedback(_feedbackPin, 2000, 1);
    } else {
      // connected to a wifi net and able to send/receive mqtt messages
      _moduleConfig.blockingFeedback(_feedbackPin, 100, 10);
    }
  }
  #ifdef LOGGING
  debug(F("Setting channels pin mode. Channels count"), _channelsCount);
  #endif
  for (int i = 0; i < _channelsCount; ++i) {
    #ifdef LOGGING
    Serial.printf("Setting pin %d of channel %s to %s mode\n", _channels[i]->pin, _channels[i]->name, _channels[i]->pinMode == OUTPUT ? "OUTPUT" : "INPUT");
    #endif
    pinMode(_channels[i]->pin, _channels[i]->pinMode);
    if (_channels[i]->pinMode == OUTPUT) {
      digitalWrite(_channels[i]->pin, _channels[i]->state);
    } else {
      _channels[i]->state = digitalRead(_channels[i]->pin);
    }
  }
  if (!_runningStandAlone) {
    #ifndef MQTT_OFF
    #ifdef LOGGING
    debug(F("Configuring MQTT broker"));
    debug(F("HOST"), getMqttServerHost());
    debug(F("PORT"), getMqttServerPort());
    #endif
    getMqttClient()->setServer(getMqttServerHost(), getMqttServerPort());
    getMqttClient()->setCallback(std::bind(&ESPDomotic::receiveMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    #endif
    _settings->loadChannelsSettings();
    // OTA Update
    #ifdef LOGGING
    debug(F("Setting OTA update"));
    #endif
    // no network connection so module cant be reached
    #ifndef ESP01
    MDNS.begin(getStationName());
    MDNS.addService("http", "tcp", 80);
    #endif
    _httpUpdater.setup(&_httpServer);
    _httpServer.begin();
    #ifdef LOGGING
    debug(F("HTTPUpdateServer ready.")); 
    debug("Open http://" + WiFi.localIP().toString() + "/update");
    #ifndef ESP01
    debug("Open http://" + String(getStationName()) + ".local/update");
    #endif
    #endif
  }
}

void ESPDomotic::saveConfig() {
  _settings->saveConfig();
}

void ESPDomotic::loop() {
  if (!_runningStandAlone) {
    _httpServer.handleClient();
    #ifndef MQTT_OFF
    if (!getMqttClient()->connected()) {
      connectBroker();
    }
    getMqttClient()->loop();
    #endif
  }
}

#ifndef MQTT_OFF
void ESPDomotic::setMqttConnectionCallback(std::function<void()> callback) {
    _mqttConnectionCallback = callback;
}

void ESPDomotic::setMqttMessageCallback(std::function<void(char*, uint8_t*, unsigned int)> callback) {
  _mqttMessageCallback = callback;
}
#endif

void ESPDomotic::setFeedbackPin(uint8_t fp) {
  _feedbackPin = fp;
}

void ESPDomotic::setModuleType(const char* type) {
  _moduleType = type;
}

void ESPDomotic::setPortalSSID (const char* ssid) {
  _apSSID = ssid;
}

void ESPDomotic::addChannel(Channel *channel) {
  if (_channelsCount < _channelsMax) {
    _channels[_channelsCount++] = channel;
  } else {
    #ifdef LOGGING
    debug(F("No more channels suported"));
    #endif
  }
}

Channel *ESPDomotic::getChannel(uint8_t i) {
  if (i <= _channelsCount) {
    return _channels[i];
  } else {
    return NULL;
  }
}

uint8_t ESPDomotic::getChannelsCount() {
  return _channelsCount;
}

#ifndef MQTT_OFF
uint16_t ESPDomotic::getMqttServerPort() {
  return (uint16_t) String(_mqttPort->getValue()).toInt();
}

const char* ESPDomotic::getMqttServerHost() {
  return _mqttHost->getValue();
}
#endif

const char* ESPDomotic::getModuleName() {
  return _moduleName->getValue();
}

const char* ESPDomotic::getModuleLocation() {
  return _moduleLocation->getValue();
}

const char* ESPDomotic::getModuleType() {
  return _moduleType;
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

#ifndef MQTT_OFF
PubSubClient* ESPDomotic::getMqttClient() {
  return &_mqttClient;
}
#endif

ESP8266WebServer* ESPDomotic::getHttpServer() {
  return &_httpServer;
}

#ifndef MQTT_OFF
void ESPDomotic::connectBroker() {
  if (_mqttNextConnAtte <= millis()) {
    _mqttNextConnAtte = millis() + _mqttBrokerReconnectionRetry;
    #ifdef LOGGING
    debug(F("Connecting MQTT broker as"), getStationName());
    #endif
    if (getMqttClient()->connect(getStationName())) {
      #ifdef LOGGING
      debug(F("MQTT broker Connected"));
      #endif
      homieSignUp();
      subscribe();
    } else {
      #ifdef LOGGING
      debug(F("Failed. RC:"), getMqttClient()->state());
      #endif
    }
  }
}

void ESPDomotic::homieSignUp() {
  if (_fwName) {
    #ifdef LOGGING
    debug(F("Homie sign up"));
    #endif
    // /homie/deviceID
    debug(F("Device details"));
    String baseTopic = String("homie/") + String(getModuleType()) + "_pepelui";
    publish(String(baseTopic + "/$homie").c_str(), _homieVersion);
    //Friendly name
    publish(String(baseTopic + "/$name").c_str(), getModuleName());
    // publish(String(baseTopic + "/$localip").c_str(), "192.168.1.104");
    publish(String(baseTopic + "/$localip").c_str(), toStringIp(WiFi.localIP()).c_str());
    publish(String(baseTopic + "/$mac").c_str(), WiFi.macAddress().c_str());
    publish(String(baseTopic + "/$fw/name").c_str(), _fwName);
    publish(String(baseTopic + "/$fw/version").c_str(), _fwVersion);
    //Iterate channels and build a comma separated list of them
    String nodes = "";
    for (uint8_t i = 0; i < _channelsCount; ++i) {
      nodes += _channels[i]->id;
    }
    publish(String(baseTopic + "/$nodes").c_str(), nodes.c_str());
    publish(String(baseTopic + "/$implementation").c_str(), _implementation);
    publish(String(baseTopic + "/$state").c_str(), "init");
    debug(F("Nodes details"));
    //Iterate channels and send its properties. Each channel is a node.
    for (uint8_t i = 0; i < _channelsCount; ++i) {
      String channelTopic = baseTopic + "/" + String(_channels[i]->id);
      publish(String(channelTopic + "/$name").c_str(), _channels[i]->name); // required
      //TODO Decidir si dejar al usuario configurar el tipo de nodo (canal) o usar el "module type"
      publish(String(channelTopic + "/$type").c_str(), getModuleType()); // required
      // publish(String(channelTopic + "/$array").c_str(), "on"); // required if the node is an array
      debug(F("Node properties"));
      publish(String(channelTopic + "/$properties").c_str(), "on"); // required
      //Specification of each property
      String propertyTopic = channelTopic + "/" + String(_channels[i]->property()->getId());
      publish(String(propertyTopic + "/$name").c_str(), _channels[i]->property()->getName()); // not required default ""
      publish(String(propertyTopic + "/$settable").c_str(), _channels[i]->property()->isSettable() ? "true" : "false"); // not required default false
      publish(String(propertyTopic + "/$retained").c_str(), _channels[i]->property()->isRetained() ? "true" : "false"); // not required default true
      publish(String(propertyTopic + "/$datatype").c_str(), _channels[i]->property()->getDataType()); // not required default String
      if (_channels[i]->property()->getUnit() && _channels[i]->property()->getUnit()[0] != '\0') {
        publish(String(propertyTopic + "/$unit").c_str(), _channels[i]->property()->getUnit()); // not required default ""
      }
      if (_channels[i]->property()->getFormat() && _channels[i]->property()->getFormat()[0] != '\0') {
        publish(String(propertyTopic + "/$format").c_str(), _channels[i]->property()->getFormat()); // required just for color and enum
      }
    }
  } else {
    debug("✖ Firmware was not set. Not advertising device.");
  }
}

bool ESPDomotic::publish(const char* topic, const char* payload) {
  return publish(topic, payload, true);
}

bool ESPDomotic::publish(const char* topic, const char* payload, bool retained) {
  #ifdef LOGGING
  Serial.printf("MQTT Publishing: [%s=%s]", topic, payload);
  Serial.println();
  #endif
  return getMqttClient()->publish(topic, payload, retained);
}

void ESPDomotic::subscribe() {
  // subscribe station to any command
  String topic = getStationTopic("command/+");
  getMqttClient()->subscribe(topic.c_str(), _qos);
  #ifdef LOGGING
  debug(F("Subscribed to"), topic.c_str());
  #endif
  // subscribe channels to any command
  for (size_t i = 0; i < getChannelsCount(); ++i) {
    topic = getChannelTopic(getChannel(i), "command/+");
    #ifdef LOGGING
    debug(F("Subscribed to"), topic.c_str());
    #endif
    getMqttClient()->subscribe(topic.c_str(), _qos);
  }
  if (_mqttConnectionCallback) {
    _mqttConnectionCallback();
  }
}

String ESPDomotic::toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

String ESPDomotic::getChannelTopic (Channel *channel, String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + channel->name + F("/") + suffix;
}

String ESPDomotic::getStationTopic (String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + suffix;
}

void ESPDomotic::setFirmware (const char *name, const char *version) {
  _fwName = name;
  _fwVersion = version;
}
#endif

bool ESPDomotic::openChannel (Channel* channel) {
  #ifdef LOGGING
  debug(F("Opening channel"), channel->name);
  #endif
  if (channel->state == LOW) {
    #ifdef LOGGING
    debug(F("Channel already opened, skipping"));
    #endif
    return false;
  } else {
    #ifdef LOGGING
    debug(F("Changing state to [ON]"));
    #endif
    digitalWrite(channel->pin, LOW);
    channel->state = LOW;
    return true;
  }
}

bool ESPDomotic::closeChannel (Channel* channel) {
  #ifdef LOGGING
  debug(F("Closing channel"), channel->name);
  #endif
  if (channel->state == HIGH) {
    #ifdef LOGGING
    debug(F("Channel already closed, skipping"));
    #endif
    return false;
  } else {
    #ifdef LOGGING
    debug(F("Changing state to [OFF]"));
    #endif
    digitalWrite(channel->pin, HIGH);
    channel->state = HIGH;
    return true;
  }
}

#ifndef MQTT_OFF
void ESPDomotic::receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("MQTT message received on topic"), topic);
  #endif
  if (String(topic).equals(getStationTopic("command/hrst"))) {
    moduleHardReset();
  } else {
    for (size_t i = 0; i < getChannelsCount(); ++i) {
      Channel *channel = getChannel(i);
      if (getChannelTopic(channel, "command/enable").equals(topic)) {
        if (enableChannel(channel, payload, length)) {
          _settings->saveChannelsSettings();
        }
        publish(getChannelTopic(channel, "feedback/enabled").c_str(), channel->enabled ? "1" : "0");
      } else if (getChannelTopic(channel, "command/timer").equals(topic)) {
        if (updateChannelTimer(channel, payload, length)) {
          _settings->saveChannelsSettings();
        }
      } else if (getChannelTopic(channel, "command/rename").equals(topic)) {
        if (renameChannel(channel, payload, length)) {
          _settings->saveChannelsSettings();
        }
      } else if (channel->pinMode == OUTPUT && getChannelTopic(channel, "command/state").equals(topic)) {
        // command/state topic is used to change the state on the channel with a desired value. So, receiving a mqtt
        // message with this purpose has sense only if the channel is an output one.
        changeState(channel, payload, length);
        publish(getChannelTopic(channel, "feedback/state").c_str(), channel->state == LOW ? "1" : "0");
      }
    }
  }
  if (_mqttMessageCallback) {
    #ifdef LOGGING
    debug(F("Passing mqtt callback to user"));
    #endif
    _mqttMessageCallback(topic, payload, length);
  }
}
#endif

void ESPDomotic::moduleHardReset () {
  #ifdef LOGGING
  debug(F("Doing a module hard reset"));
  #endif
  _settings->format();
  WiFi.disconnect();
  delay(200);
  ESP.restart();
}

void ESPDomotic::setWifiConnectTimeout (uint16_t seconds) {
  _wifiConnectTimeout = seconds;
}

void ESPDomotic::setConfigPortalTimeout (uint16_t seconds) {
  _configPortalTimeout = seconds;
}

void ESPDomotic::setConfigFileSize (uint16_t bytes) {
  _configFileSize = bytes;
}

bool ESPDomotic::enableChannel(Channel* channel, unsigned char* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Ubpading channel enablement"), channel->name);
  #endif
  if (length != 1 || !payload) {
    #ifdef LOGGING
    debug(F("Invalid payload. Ignoring."));
    #endif
    return false;
  }
  bool stateChanged = false;
  switch (payload[0]) {
    case '0':
      stateChanged = channel->enabled;
      channel->enabled = false;
      break;
    case '1':
      stateChanged = !channel->enabled;
      channel->enabled = true;
      break;
    default:
      #ifdef LOGGING
      debug(F("Invalid state"), payload[0]);
      #endif
      break;
  }
  return stateChanged;
}

bool ESPDomotic::renameChannel(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Updating channel name"), channel->name);
  #endif
  if (length < 1) {
    #ifdef LOGGING
    debug(F("Invalid payload"));
    #endif
    return false;
  }
  char newName[length + 1];
  for (uint16_t i = 0 ; i < length; ++ i) {
    newName[i] = payload[i];
  }
  newName[length] = '\0';
  bool renamed = !String(channel->name).equals(String(newName));
  if (renamed) {
    #ifdef LOGGING
    debug(F("Channel renamed"), newName);
    #endif
    #ifndef MQTT_OFF
    getMqttClient()->unsubscribe(getChannelTopic(channel, "command/+").c_str());
    channel->updateName(newName);
    getMqttClient()->subscribe(getChannelTopic(channel, "command/+").c_str());
    #endif
  }
  return renamed;
}

bool ESPDomotic::changeState(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Updating channel state"), channel->name);
  #endif
  if (length < 1) {
    #ifdef LOGGING
    debug(F("Invalid payload"));
    #endif
    return false;
  }
  switch (payload[0]) {
    case '0':
      return closeChannel(channel);
    case '1':
      return openChannel(channel);
    default:
      #ifdef LOGGING
      debug(F("Invalid state"), payload[0]);
      #endif
    return false;
  }
}

bool ESPDomotic::updateChannelTimer(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Updating irrigation duration for channel"), channel->name);
  #endif
  if (length < 1) {
    #ifdef LOGGING
    debug(F("Invalid payload"));
    #endif
    return false;
  }
  char buff[length + 1];
  for (uint16_t i = 0 ; i < length; ++ i) {
    buff[i] = payload[i];
  }
  buff[length] = '\0';
  #ifdef LOGGING
  debug(F("New duration for channel"), channel->name);
  #endif
  long newTimer = String(buff).toInt();
  #ifdef LOGGING
  debug(F("Duration"), newTimer);
  #endif
  bool timerChanged = channel->timer != (unsigned long) newTimer * 60 * 1000;
  channel->timer = newTimer * 60 * 1000; // received in minutes set in millis
  return timerChanged;
}