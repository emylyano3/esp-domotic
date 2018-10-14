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
  debug(F("ESP Domotic module INIT"));
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
    _moduleConfig.setFeedbackPin(_feedbackPin);
  }
  _moduleConfig.connectWifiNetwork(loadConfig());
  if (_feedbackPin != _invalidPinNo) {
    _moduleConfig.blockingFeedback(_feedbackPin, 100, 8);
  }
  // Channels pin mode
  for (int i = 0; i > _channelsCount; ++i) {
    pinMode(_channels[i]->pin, _channels[i]->pinMode);
    digitalWrite(_channels[i].pin, HIGH);
  }
  debug(F("Connected to wifi...."));
  // MQTT Server config
  debug(F("Configuring MQTT broker"));
  debug(F("Port"), getMqttServerPort());
  debug(F("Server"), getMqttServerHost());
  _mqttClient.setServer(getMqttServerHost(), getMqttServerPort());
  _mqttClient.setCallback(std::bind(&ESPDomotic::receiveMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  loadChannelsSettings();
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

void ESPDomotic::addChannel(Channel *c) {
  if (_channelsCount < _channelsMax) {
    _channels[_channelsCount++] = c;
  } else {
    debug(F("No more channels suported"));
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
  if (SPIFFS.begin()) {
    if (SPIFFS.exists(fileName)) {
      File file = SPIFFS.open(fileName, "r");
      if (file) {
        file.readBytes(buff, size);
        file.close();
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
}

void ESPDomotic::connectBroker() {
  if (_mqttNextConnAtte <= millis()) {
    _mqttNextConnAtte = millis() + _mqttBrokerReconnectionRetry;
    debug(F("Connecting MQTT broker as"), getStationName());
    if (_mqttClient.connect(getStationName())) {
      debug(F("MQTT broker Connected"));
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
      debug(F("Failed. RC:"), _mqttClient.state());
    }
  }
}

String ESPDomotic::getChannelTopic (Channel *c, String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + c->name + F("/") + suffix;
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

bool ESPDomotic::loadChannelsSettings () {
  if (_channelsCount > 0) {
    size_t size = getFileSize("/settings.json");
    if (size > 0) {
      char buff[size];
      loadFile("/settings.json", buff, size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buff);
      if (_debug) {
        json.printTo(Serial);
        Serial.println();
      }
      if (json.success()) {
        for (uint8_t i = 0; i < _channelsCount; ++i) {
          _channels[i]->updateName(json[String(_channels[i]->id) + "_name"]);
          _channels[i]->timer = json[String(_channels[i]->id) + "_timer"];
          _channels[i]->enabled = json[String(_channels[i]->id) + "_enabled"];
          debug(F("Channel id"), _channels[i]->id);
          debug(F("Channel name"), _channels[i]->name);
          debug(F("Channel timer"), _channels[i]->timer);
          debug(F("Channel enabled"), _channels[i]->enabled);
        }
        return true;
      } else {
        debug(F("Failed to load json"));
      }
    }
  } else {
    debug(F("No channel configured"));
  }
  return false;
}

void ESPDomotic::openChannel (Channel* c) {
  debug(F("Opening channel"), c->name);
  if (c->state == LOW) {
    debug(F("Valve already opened, skipping"));
  } else {
    digitalWrite(c->pin, LOW);
    c->state = LOW;
  }
}

void ESPDomotic::closeChannel (Channel* c) {
  debug(F("Closing channel"), c->name);
  if (c->state == HIGH) {
    debug(F("Valve already closed, skipping"));
  } else {
    digitalWrite(c->pin, HIGH);
    c->state = HIGH;
  }
}

void ESPDomotic::receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  debug("MQTT message received on topic", topic);
  if (String(topic).equals(getStationTopic("command/hrst"))) {
    moduleHardReset();
  } else {
    for (size_t i = 0; i < getChannelsCount(); ++i) {
      Channel *c = getChannel(i);
      if (getChannelTopic(c, "command/enable").equals(topic)) {
        if (enableChannel(c, payload, length)) {
          saveChannelsSettings();
        }
        getMqttClient()->publish(getChannelTopic(c, "feedback/enabled").c_str(), c->enabled ? "1" : "0");
      } else if (getChannelTopic(c, "command/timer").equals(topic)) {
        if (updateChannelTimer(c, payload, length)) {
          saveChannelsSettings();
        }
      } else if (getChannelTopic(c, "command/rename").equals(topic)) {
        if (renameChannel(c, payload, length)) {
          saveChannelsSettings();
        }
      } else if (c->pinMode == OUTPUT && getChannelTopic(c, "command/state").equals(topic)) {
        // command/state topic is used to change the state on the channel with a desired value. So, receiving a mqtt
        // message with this purpose has sense only if the channel is an output one.
        changeState(c, payload, length);
        getMqttClient()->publish(getChannelTopic(c, "feedback/state").c_str(), c->state == HIGH ? "1" : "0");
      }
    }
  }
  if (_mqttMessageCallback) {
    debug("Passing mqtt callback to user");
    _mqttMessageCallback(topic, payload, length);
  }
}

void ESPDomotic::moduleHardReset () {
  debug(F("Doing a module hard reset"));
  SPIFFS.format();
  WiFi.disconnect();
  delay(200);
  ESP.restart();
}

bool ESPDomotic::enableChannel(Channel* c, unsigned char* payload, unsigned int length) {
  debug(F("Ubpading channel enablement"), c->name);
  if (length != 1 || !payload) {
    debug(F("Invalid payload. Ignoring."));
    return false;
  }
  bool stateChanged = false;
  switch (payload[0]) {
    case '0':
      stateChanged = c->enabled;
      c->enabled = false;
      break;
    case '1':
      stateChanged = !c->enabled;
      c->enabled = true;
      break;
    default:
      debug(F("Invalid state"), payload[0]);
      break;
  }
  return stateChanged;
}

bool ESPDomotic::renameChannel(Channel* c, uint8_t* payload, unsigned int length) {
  debug(F("Updating channel name"), c->name);
  if (length < 1) {
    debug(F("Invalid payload"));
    return false;
  }
  char newName[length + 1];
  for (uint16_t i = 0 ; i < length; ++ i) {
    newName[i] = payload[i];
  }
  newName[length] = '\0';
  bool renamed = !String(c->name).equals(String(newName));
  if (renamed) {
    debug(F("Channel renamed"), newName);
    getMqttClient()->unsubscribe(getChannelTopic(c, "command/+").c_str());
    c->updateName(newName);
    getMqttClient()->subscribe(getChannelTopic(c, "command/+").c_str());
  }
  return renamed;
}

bool ESPDomotic::changeState(Channel* c, uint8_t* payload, unsigned int length) {
  debug(F("Updating channel state"), c->name);
  if (length < 1) {
    debug(F("Invalid payload"));
    return false;
  }
  switch (payload[0]) {
    case '0':
      if (c->state == LOW) {
        debug("State same as before [LOW]");
        return false;
      } else {
        debug("Changing state to [LOW]");
        c->state = LOW;
        digitalWrite(c->pin, LOW);
        return true;
      }
    case '1':
      if (c->state == HIGH) {
        debug("State same as before [HIGH]");
        return false;
      } else {
        debug("Changing state to [HIGH]");
        c->state = HIGH;
        digitalWrite(c->pin, HIGH);
        return true;
      }
     break;
    default:
      debug(F("Invalid state"), payload[0]);
    return false;
  }
}

bool ESPDomotic::updateChannelTimer(Channel* c, uint8_t* payload, unsigned int length) {
  debug(F("Updating irrigation duration for channel"), c->name);
  if (length < 1) {
    debug(F("Invalid payload"));
    return false;
  }
  char buff[length + 1];
  for (uint16_t i = 0 ; i < length; ++ i) {
    buff[i] = payload[i];
  }
  buff[length] = '\0';
  debug(F("New duration for channel"), c->name);
  long newTimer = String(buff).toInt();
  debug(F("Duration"), newTimer);
  bool timerChanged = c->timer != (unsigned long) newTimer * 60 * 1000;
  c->timer = newTimer * 60 * 1000; // received in minutes set in millis
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
    Serial.print("*DOMO: ");
    Serial.println(text);
  }
}

template <class T, class U> void ESPDomotic::debug (T key, U value) {
  if (_debug) {
    Serial.print("*DOMO: ");
    Serial.print(key);
    Serial.print(": ");
    Serial.println(value);
  }
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
  this->id = id;
  this->pin = pin;
  this->state = state;
  this->timer = timer;
  this->enabled = true;
  this->pinMode = pinMode;
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