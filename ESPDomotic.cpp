#include <FS.h>
#include <ESPDomotic.h>
#include <ESPConfig.h>

#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include <ArduinoJson.h>

/* Config params */
ESPConfigParam            _mqttPort (Text, "mqttPort", "MQTT port", "", _paramPortValueLength, "required");
ESPConfigParam            _mqttHost (Text, "mqttHost", "MQTT host", "", _paramIPValueLength, "required");
ESPConfigParam            _moduleName (Text, "moduleName", "Module name", "", _paramValueMaxLength, "required");
ESPConfigParam            _moduleLocation (Text, "moduleLocation", "Module location", "", _paramValueMaxLength, "required");

/* HTTP Update */
ESP8266WebServer          _httpServer(80);
ESP8266HTTPUpdateServer   _httpUpdater;

/* MQTT client */
WiFiClient                _wifiClient;
PubSubClient              _mqttClient(_wifiClient);
/* MQTT broker reconnection control */
unsigned long             _mqttNextConnAtte     = 0;

char                      _stationName[_paramValueMaxLength * 3 + 4];

const uint8_t _channelsMax    = 5;
uint8_t       _channelsCount  = 0;

ESPDomotic::ESPDomotic() {
  _channels = (Channel**)malloc(_channelsMax * sizeof(Channel*));
}

ESPDomotic::~ESPDomotic() {}

void ESPDomotic::init() {
  #ifdef LOGGING
  debug(F("ESP Domotic module INIT"));
  #endif
  /* Wifi connection */
  ESPConfig _moduleConfig;
  _moduleConfig.addParameter(&_moduleLocation);
  _moduleConfig.addParameter(&_moduleName);
  _moduleConfig.addParameter(&_mqttHost);
  _moduleConfig.addParameter(&_mqttPort);
  _moduleConfig.setConnectionTimeout(_wifiConnectTimeout);
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
  _moduleConfig.connectWifiNetwork(loadConfig());
  #ifdef LOGGING
  debug(F("Connected to wifi...."));
  #endif
  if (_feedbackPin != _invalidPinNo) {
    _moduleConfig.blockingFeedback(_feedbackPin, 100, 8);
  }
  #ifdef LOGGING
  debug("Setting channels pin mode. Channels count", _channelsCount);
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
  #ifdef LOGGING
  debug(F("Configuring MQTT broker"));
  debug(F("HOST"), getMqttServerHost());
  debug(F("PORT"), getMqttServerPort());
  #endif
  _mqttClient.setServer(getMqttServerHost(), getMqttServerPort());
  _mqttClient.setCallback(std::bind(&ESPDomotic::receiveMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  loadChannelsSettings();
  // OTA Update
  #ifdef LOGGING
  debug(F("Setting OTA update"));
  #endif
  MDNS.begin(getStationName());
  MDNS.addService("http", "tcp", 80);
  _httpUpdater.setup(&_httpServer);
  _httpServer.begin();
  #ifdef LOGGING
  debug(F("HTTPUpdateServer ready.")); 
  debug("Open http://" + String(getStationName()) + ".local/update");
  debug("Open http://" + WiFi.localIP().toString() + "/update");
  #endif
}

void ESPDomotic::loop() {
  _httpServer.handleClient();
  if (!_mqttClient.connected()) {
    connectBroker();
  }
  _mqttClient.loop();
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
        #ifdef LOGGING
        debug(F("Cant open file"), fileName);
        #endif
      }
    } else {
      #ifdef LOGGING
      debug(F("File not found"), fileName);
      #endif
    }
  } else {
    #ifdef LOGGING
    debug(F("Failed to mount FS"));
    #endif
  }
  return 0;
}

void ESPDomotic::loadFile (const char* fileName, char buff[], size_t size) {
  if (SPIFFS.begin()) {
    if (SPIFFS.exists(fileName)) {
      File file = SPIFFS.open(fileName, "r");
      if (file) {
        file.readBytes(buff, size);
        file.close();
      } else {
        file.close();
        #ifdef LOGGING
        debug(F("Cant open file"), fileName);
        #endif
      }
    } else {
      #ifdef LOGGING
      debug(F("File not found"), fileName);
      #endif
    }
  } else {
    #ifdef LOGGING
    debug(F("Failed to mount FS"));
    #endif
  }
}

void ESPDomotic::connectBroker() {
  if (_mqttNextConnAtte <= millis()) {
    _mqttNextConnAtte = millis() + _mqttBrokerReconnectionRetry;
    #ifdef LOGGING
    debug(F("Connecting MQTT broker as"), getStationName());
    #endif
    if (_mqttClient.connect(getStationName())) {
      #ifdef LOGGING
      debug(F("MQTT broker Connected"));
      #endif
      // subscribe station to any command
      getMqttClient()->subscribe(getStationTopic("command/+").c_str());
      // subscribe channels to any command
      for (size_t i = 0; i < getChannelsCount(); ++i) {
        getMqttClient()->subscribe(getChannelTopic(getChannel(i), "command/+").c_str());
      }
      if (_mqttConnectionCallback) {
        _mqttConnectionCallback();
      }
    } else {
      #ifdef LOGGING
      debug(F("Failed. RC:"), _mqttClient.state());
      #endif
    }
  }
}

String ESPDomotic::getChannelTopic (Channel *channel, String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + channel->name + F("/") + suffix;
}

String ESPDomotic::getStationTopic (String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + suffix;
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
      #ifdef LOGGING
      json.printTo(Serial);
      Serial.println();
      #endif
      return true;
    } else {
      #ifdef LOGGING
      debug(F("Failed to load json config"));
      #endif
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
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    json.printTo(Serial);
    Serial.println();
    #endif
    file.close();
  } else {
    #ifdef LOGGING
    debug(F("Failed to open config file for writing"));
    #endif
  }
}

bool ESPDomotic::loadChannelsSettings () {
  if (_channelsCount > 0) {
    size_t size = getFileSize("/settings.json");
    if (size > 0) {
      char buff[size];
      loadFile("/settings.json", buff, size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buff);
      #ifdef LOGGING
      json.printTo(Serial);
      Serial.println();
      #endif
      if (json.success()) {
        for (uint8_t i = 0; i < _channelsCount; ++i) {
          _channels[i]->updateName(json[String(_channels[i]->id) + "_name"]);
          _channels[i]->timer = json[String(_channels[i]->id) + "_timer"];
          _channels[i]->enabled = json[String(_channels[i]->id) + "_enabled"];
          #ifdef LOGGING
          debug(F("Channel id"), _channels[i]->id);
          debug(F("Channel name"), _channels[i]->name);
          debug(F("Channel timer"), _channels[i]->timer);
          debug(F("Channel enabled"), _channels[i]->enabled);
          #endif
        }
        return true;
      } else {
        #ifdef LOGGING
        debug(F("Failed to load json"));
        #endif
      }
    }
  } else {
    #ifdef LOGGING
    debug(F("No channel configured"));
    #endif
  }
  return false;
}

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
    debug("Changing state to [ON]");
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
    debug("Changing state to [OFF]");
    #endif
    digitalWrite(channel->pin, HIGH);
    channel->state = HIGH;
    return true;
  }
}

void ESPDomotic::receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug("MQTT message received on topic", topic);
  #endif
  if (String(topic).equals(getStationTopic("command/hrst"))) {
    moduleHardReset();
  } else {
    for (size_t i = 0; i < getChannelsCount(); ++i) {
      Channel *channel = getChannel(i);
      if (getChannelTopic(channel, "command/enable").equals(topic)) {
        if (enableChannel(channel, payload, length)) {
          saveChannelsSettings();
        }
        getMqttClient()->publish(getChannelTopic(channel, "feedback/enabled").c_str(), channel->enabled ? "1" : "0");
      } else if (getChannelTopic(channel, "command/timer").equals(topic)) {
        if (updateChannelTimer(channel, payload, length)) {
          saveChannelsSettings();
        }
      } else if (getChannelTopic(channel, "command/rename").equals(topic)) {
        if (renameChannel(channel, payload, length)) {
          saveChannelsSettings();
        }
      } else if (channel->pinMode == OUTPUT && getChannelTopic(channel, "command/state").equals(topic)) {
        // command/state topic is used to change the state on the channel with a desired value. So, receiving a mqtt
        // message with this purpose has sense only if the channel is an output one.
        changeState(channel, payload, length);
        getMqttClient()->publish(getChannelTopic(channel, "feedback/state").c_str(), channel->state == LOW ? "1" : "0");
      }
    }
  }
  if (_mqttMessageCallback) {
    #ifdef LOGGING
    debug("Passing mqtt callback to user");
    #endif
    _mqttMessageCallback(topic, payload, length);
  }
}

void ESPDomotic::moduleHardReset () {
  #ifdef LOGGING
  debug(F("Doing a module hard reset"));
  #endif
  SPIFFS.format();
  WiFi.disconnect();
  delay(200);
  ESP.restart();
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
    getMqttClient()->unsubscribe(getChannelTopic(channel, "command/+").c_str());
    channel->updateName(newName);
    getMqttClient()->subscribe(getChannelTopic(channel, "command/+").c_str());
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

void ESPDomotic::saveChannelsSettings () {
  File file = SPIFFS.open("/settings.json", "w");
  if (file) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //TODO Trim param values
    for (uint8_t i = 0; i < _channelsCount; ++i) {
      json[String(_channels[i]->id) + "_name"] = _channels[i]->name;
      json[String(_channels[i]->id) + "_timer"] = _channels[i]->timer;
      json[String(_channels[i]->id) + "_enabled"] = _channels[i]->enabled;
    }
    json.printTo(file);
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    json.printTo(Serial);
    Serial.println();
    #endif
    file.close();
  } else {
    #ifdef LOGGING
    debug(F("Failed to open config file for writing"));
    #endif
  }
}

#ifdef LOGGING
template <class T> void ESPDomotic::debug (T text) {
  Serial.print("*DOMO: ");
  Serial.println(text);
}

template <class T, class U> void ESPDomotic::debug (T key, U value) {
  Serial.print("*DOMO: ");
  Serial.print(key);
  Serial.print(": ");
  Serial.println(value);
}
#endif

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state) {
  init(id, name, pin, pinMode, state, -1, NULL);
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
  init(id, name, pin, pinMode, state, timer, NULL);
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, Channel *slave) {
  init(id, name, pin, pinMode, state, -1, slave);
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer, Channel *slave) {
  init(id, name, pin, pinMode, state, timer, slave);
}

void Channel::init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer, Channel *slave) {
  this->id = id;
  this->pin = pin;
  this->state = state;
  this->timer = timer;
  this->enabled = true;
  this->pinMode = pinMode;
  this->slave = slave;
  this->name = new char[_channelNameMaxLength + 1];
  updateName(name);
}

void Channel::updateName (const char *v) {
  String(v).toCharArray(this->name, _channelNameMaxLength);
}

void Channel::updateTimerControl() {
  this->timerControl = millis() + this->timer;
}

bool Channel::isEnabled () {
  return this->enabled && this->name != NULL && strlen(this->name) > 0;
}