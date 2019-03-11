#include <FS.h>
#include <ESPDomotic.h>
#include <ESPConfig.h>

#include <ESP8266HTTPUpdateServer.h>

#ifndef ESP01
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#endif

/* Config params */
#ifndef MQTT_OFF
ESPConfigParam            _mqttPort (Text, "mqttPort", "MQTT port", "", _paramPortValueLength, "required");
ESPConfigParam            _mqttHost (Text, "mqttHost", "MQTT host", "", _paramIPValueLength, "required");
#endif
ESPConfigParam            _moduleName (Text, "moduleName", "Module name", "", _paramValueMaxLength, "required");
ESPConfigParam            _moduleLocation (Text, "moduleLocation", "Module location", "", _paramValueMaxLength, "required");

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
  #ifndef MQTT_OFF
  _moduleConfig.addParameter(&_mqttHost);
  _moduleConfig.addParameter(&_mqttPort);
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
  _runningStandAlone = !_moduleConfig.connectWifiNetwork(loadConfig());
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
    _mqttClient.setServer(getMqttServerHost(), getMqttServerPort());
    _mqttClient.setCallback(std::bind(&ESPDomotic::receiveMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    #endif
    loadChannelsSettings();
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

void ESPDomotic::loop() {
  if (!_runningStandAlone) {
    _httpServer.handleClient();
    #ifndef MQTT_OFF
    if (!_mqttClient.connected()) {
      connectBroker();
    }
    _mqttClient.loop();
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
  return (uint16_t) String(_mqttPort.getValue()).toInt();
}

const char* ESPDomotic::getMqttServerHost() {
  return _mqttHost.getValue();
}
#endif

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

#ifndef MQTT_OFF
PubSubClient* ESPDomotic::getMqttClient() {
  return &_mqttClient;
}
#endif

ESP8266WebServer* ESPDomotic::getHttpServer() {
  return &_httpServer;
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

#ifndef MQTT_OFF
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
      String topic = getStationTopic("command/+");
      getMqttClient()->subscribe(topic.c_str());
      #ifdef LOGGING
      debug(F("Subscribed to"), topic.c_str());
      #endif
      // subscribe channels to any command
      for (size_t i = 0; i < getChannelsCount(); ++i) {
        topic = getChannelTopic(getChannel(i), "command/+");
        #ifdef LOGGING
        debug(F("Subscribed to"), topic.c_str());
        #endif
        getMqttClient()->subscribe(topic.c_str());
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
#endif

#ifndef MQTT_OFF
String ESPDomotic::getChannelTopic (Channel *channel, String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + channel->name + F("/") + suffix;
}

String ESPDomotic::getStationTopic (String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + suffix;
}
#endif

bool ESPDomotic::loadConfig () {
  size_t size = getFileSize("/config.json");
  if (size > 0) {
    #ifndef ESP01
    char buff[size];
    loadFile("/config.json", buff, size);
    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, buff);
    if (!error) {
      #ifndef MQTT_OFF
      _mqttHost.updateValue(doc[_mqttHost.getName()]);
      _mqttPort.updateValue(doc[_mqttPort.getName()]);
      #endif
      _moduleLocation.updateValue(doc[_moduleLocation.getName()]);
      _moduleName.updateValue(doc[_moduleName.getName()]);
      #ifdef LOGGING
      serializeJsonPretty(doc, Serial);
      #endif
      return true;
    } else {
      #ifdef LOGGING
      debug(F("Failed to load json config"), error.c_str());
      #endif
      return false;
    }
    #else
    // Avoid using json to reduce build size
    File configFile = SPIFFS.open("/config.json", "r");
    bool readOK = true;
    while (readOK && configFile.position() < size) {
      String line = configFile.readStringUntil('\n');
      line.trim();
      size_t ioc = line.indexOf('=');
      if (ioc >= 0 && ioc + 1 < line.length()) {
        String key = line.substring(0, ioc++);
        String val = line.substring(ioc, line.length());
        #ifdef LOGGING
        debug(F("Read key"), key);
        debug(F("Key value"), val);
        #endif
        #ifndef MQTT_OFF
        if (key.equals(_mqttPort.getName())) {
          _mqttPort.updateValue(val.c_str());
        } else if (key.equals(_mqttHost.getName())) {
          _mqttHost.updateValue(val.c_str());
        } else
        #endif
        if (key.equals(_moduleLocation.getName())) {
          _moduleLocation.updateValue(val.c_str());
        } else if (key.equals(_moduleName.getName())) {
          _moduleName.updateValue(val.c_str());
        } else {
          #ifdef LOGGING
          debug(F("ERROR. Unknown key"));
          #endif
          readOK = false;
        }
      } else {
        #ifdef LOGGING
        debug(F("Config bad format"), line);
        #endif
        readOK = false;
      }
    }
    configFile.close();
    return readOK;
    #endif
  }
  return false;
}

/** callback notifying the need to save config */
void ESPDomotic::saveConfig () {
  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    #ifndef ESP01
    DynamicJsonDocument doc(200);
    //TODO Trim param values
    #ifndef MQTT_OFF
    doc[_mqttHost.getName()] = _mqttHost.getValue();
    doc[_mqttPort.getName()] = _mqttPort.getValue();
    #endif
    doc[_moduleLocation.getName()] = _moduleLocation.getValue();
    doc[_moduleName.getName()] = _moduleName.getValue();
    serializeJson(doc, file);
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    serializeJsonPretty(doc, Serial);
    #endif
    #else
    String line = String(_moduleLocation.getName()) + "=" + String(_moduleLocation.getValue());
    file.println(line);
    line = String(_moduleName.getName()) + "=" + String(_moduleName.getValue());
    file.println(line);
    #ifndef MQTT_OFF
    line = String(_mqttHost.getName()) + "=" + String(_mqttHost.getValue());
    file.println(line);
    line = String(_mqttPort.getName()) + "=" + String(_mqttPort.getValue());
    file.println(line);
    #endif
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
      #ifndef ESP01
      char buff[size];
      loadFile("/settings.json", buff, size);
      DynamicJsonDocument doc(200);
      DeserializationError error = deserializeJson(doc, buff);
      #ifdef LOGGING
      serializeJsonPretty(doc, Serial);
      #endif
      if (!error) {
        for (uint8_t i = 0; i < _channelsCount; ++i) {
          _channels[i]->updateName(doc[String(_channels[i]->id) + "_name"]);
          _channels[i]->timer = doc[String(_channels[i]->id) + "_timer"];
          _channels[i]->enabled = doc[String(_channels[i]->id) + "_enabled"];
          #ifdef LOGGING
          serializeJsonPretty(doc, Serial);
          #endif
        }
        return true;
      } else {
        #ifdef LOGGING
        debug(F("Failed to load json"), error.c_str());
        #endif
        return false;
      }
      #else
      // Avoid using json to reduce build size
      File configFile = SPIFFS.open("/settings.json", "r");
      bool readOK = true;
      while (readOK && configFile.position() < size) {
        String line = configFile.readStringUntil('\n');
        line.trim();
        size_t ioc = line.indexOf('=');
        if (ioc >= 0 && ioc + 1 < line.length()) {
          String key = line.substring(0, ioc++);
          String val = line.substring(ioc, line.length());
          #ifdef LOGGING
          debug(F("Read key"), key);
          debug(F("Key value"), val);
          #endif
          for (uint8_t i = 0; i < _channelsCount; ++i) {
            if (key.startsWith(String(_channels[i]->id) + "_")) {
              if (key.endsWith("name")) {
                _channels[i]->updateName(val.c_str());
              } else if (key.endsWith("timer")) {
                _channels[i]->timer = val.toInt();
              } else if (key.endsWith("enabled")) {
                _channels[i]->enabled = val.equals("1");
              }
            } 
          }
        } else {
          #ifdef LOGGING
          debug(F("Config bad format"), line);
          #endif
          readOK = false;
        }
      }
      configFile.close();
      return readOK;
      #endif
    }
    return false;
  } else {
    #ifdef LOGGING
    debug(F("No channel configured"));
    #endif
    return false;
  }
}

void ESPDomotic::saveChannelsSettings () {
  File file = SPIFFS.open("/settings.json", "w");
  if (file) {
    #ifndef ESP01
    DynamicJsonDocument doc(200);
    //TODO Trim param values
    for (uint8_t i = 0; i < _channelsCount; ++i) {
      doc[String(_channels[i]->id) + "_name"] = _channels[i]->name;
      doc[String(_channels[i]->id) + "_timer"] = _channels[i]->timer;
      doc[String(_channels[i]->id) + "_enabled"] = _channels[i]->enabled;
    }
    serializeJson(doc, file);
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    serializeJsonPretty(doc, Serial);
    #endif
    #else
    for (uint8_t i = 0; i > _channelsCount; ++i) {
      String line = String(_channels[i]->id) + "_name=" + String(_channels[i]->name);
      file.println(line);
      line = String(_channels[i]->id) + "_timer=" + String(_channels[i]->timer);
      file.println(line);
      line = String(_channels[i]->id) + "_enabled=" + String(_channels[i]->enabled);
      file.println(line);
    }
    #endif
    file.close();
  } else {
    #ifdef LOGGING
    debug(F("Failed to open config file for writing"));
    #endif
  }
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
  SPIFFS.format();
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
  init(id, name, pin, pinMode, state, -1);
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
  init(id, name, pin, pinMode, state, timer);
}

void Channel::init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
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