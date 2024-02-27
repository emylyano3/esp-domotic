#include <LittleFS.h>
#include <ESPDomotic.h>
#include <ESPConfig.h>

#include <ESP8266HTTPUpdateServer.h>

#ifndef ESP01
#include <ESP8266mDNS.h>
#endif
#ifdef USE_JSON
#include <ArduinoJson.h>
#endif

//TODO Pasar todo String a std::string
#include <string>

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
unsigned int              _mqttReconnections    = 0;
#endif 

char                      _stationName[_paramValueMaxLength * 3 + 4];

bool            _runningStandAlone    = false;
uint8_t         _channelsCount        = 0;

uint16_t        _wifiConnectTimeout   = 30;
uint16_t        _configPortalTimeout  = 60;
uint16_t        _configFileSize       = 200;

ESPDomotic::ESPDomotic() {
}

ESPDomotic::~ESPDomotic() {}

void ESPDomotic::init() {
  #ifdef LOGGING
  debug(F("ESP Domotic module INIT"));
  #endif
  /* Wifi connection */
  ESPConfig* _moduleConfig = new ESPConfig;
  _moduleConfig->addParameter(&_moduleLocation);
  _moduleConfig->addParameter(&_moduleName);
  #ifndef MQTT_OFF
  _moduleConfig->addParameter(&_mqttHost);
  _moduleConfig->addParameter(&_mqttPort);
  #endif
  _moduleConfig->setWifiConnectTimeout(_wifiConnectTimeout);
  _moduleConfig->setConfigPortalTimeout(_configPortalTimeout);
  _moduleConfig->setAPStaticIP(IPAddress(10,10,10,10),IPAddress(IPAddress(10,10,10,10)),IPAddress(IPAddress(255,255,255,0)));
  if (_apSSID) {
    _moduleConfig->setPortalSSID(_apSSID);
  } else {
    String ssid = "Proeza domotic " + String(ESP.getChipId());
    _moduleConfig->setPortalSSID(ssid.c_str());
  }
  _moduleConfig->setMinimumSignalQuality(_wifiMinSignalQuality);
  _moduleConfig->setStationNameCallback(std::bind(&ESPDomotic::getStationName, this));
  _moduleConfig->setSaveConfigCallback(std::bind(&ESPDomotic::saveConfig, this));
  if (_feedbackPin != _invalidPinNo) {
    pinMode(_feedbackPin, OUTPUT);
    _moduleConfig->setFeedbackPin(_feedbackPin);
  }
  _runningStandAlone = !_moduleConfig->connectWifiNetwork(loadConfig());
  #ifdef LOGGING
  debug(F("Connected to wifi"), _runningStandAlone ? "false" : "true");
  #endif
  if (_feedbackPin != _invalidPinNo) {
    if (_runningStandAlone) {
      // could not connect to a wifi net
      _moduleConfig->blockingFeedback(_feedbackPin, 2000, 1);
    } else {
      // connected to a wifi net and able to send/receive mqtt messages
      _moduleConfig->blockingFeedback(_feedbackPin, 100, 10);
    }
  }
  delete _moduleConfig;
  #ifdef LOGGING
  debug(F("Setting channels pin mode. Channels count"), _channelsCount);
  #endif
  for (int i = 0; i < _channelsCount; ++i) {
    #ifdef LOGGING
    Serial.printf("Setting pin %d of channel %s to %s mode\n", _channels[i]->pin, _channels[i]->name, _channels[i]->isOutput() ? "OUTPUT" : "INPUT");
    #endif
    pinMode(_channels[i]->pin, _channels[i]->pinMode);
    if (_channels[i]->isOutput()) {
      _channels[i]->write(_channels[i]->currState);
    } else {
      _channels[i]->read();
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
    if (!_mqttClient.loop()) {
      connectBroker();
    }
    #endif
  }
  checkOutputChannelsTimers();
  checkInputChannels();
}

void ESPDomotic::checkOutputChannelsTimers() {
  if (_behaviourLocked) {
    return;
  }
  for (size_t i = 0; i < getChannelsCount(); ++i) {
    Channel *channel = getChannel(i);
    // Timer is checked just if the channel state was changed from the logic inside this lib (locally changed)
    if (channel->isOutput() && channel->timeIsUp()) {
      #ifdef LOGGING
      debug("Timer on output channel is up", channel->name);
      #endif
      // Flip the channel state
      updateChannelState(channel, channel->prevState);
    }
  }
}

void ESPDomotic::checkInputChannels() {
  for (size_t i = 0; i < getChannelsCount(); ++i) {
    Channel *channel = getChannel(i);
    if (channel->read()) {
      String topic = getChannelTopic(channel, "feedback/state");
      // uint32_t mapped = map(_sensorChannel.state, AIR_VALUE, WATER_VALUE, 0, 100);
      // const char *payload = std::to_string(mapped).c_str();
      const char *payload = std::to_string(channel->currState).c_str();
      getMqttClient()->publish(topic.c_str(), payload);
    }
  }
}

#ifndef MQTT_OFF
void ESPDomotic::connectBroker() {
  if (_mqttNextConnAtte <= millis() && _mqttReconnections++ < _mqtt_reconnection_max_retries) {
    _mqttNextConnAtte = millis() + _mqtt_reconnection_retry_wait_millis;
    #ifdef LOGGING
    debug(F("Connecting MQTT broker as"), getStationName());
    #endif
    if (_mqttClient.connect(getStationName())) {
      _mqttReconnections = 0;
      #ifdef LOGGING
      debug(F("MQTT broker Connected"));
      #endif
      // subscribe station to any command
      String topic = getStationTopic("command/#");
      _mqttClient.subscribe(topic.c_str());
      #ifdef LOGGING
      debug(F("Subscribed to"), topic.c_str());
      #endif
      // subscribe channels to any command
      for (size_t i = 0; i < getChannelsCount(); ++i) {
        topic = getChannelTopic(getChannel(i), "command/+");
        #ifdef LOGGING
        debug(F("Subscribed to"), topic.c_str());
        #endif
        _mqttClient.subscribe(topic.c_str());
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
void ESPDomotic::receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  String sTopic = String(topic);
  #ifdef LOGGING
  debug(F("MQTT message received on topic"), sTopic);
  #endif
  if (getStationTopic("command/hrst").equals(sTopic)) {
    moduleHardReset();
  } else if (getStationTopic("command/rst").equals(sTopic)) {
    moduleSoftReset();
  } else {
    for (size_t i = 0; i < getChannelsCount(); ++i) {
      Channel *channel = getChannel(i);
      if (sTopic.endsWith(String(channel->name) + F("/command/enable"))) {
        if (enableChannelCommand(channel, payload, length)) {
          saveChannelsSettings();
        }
        _mqttClient.publish(getChannelTopic(channel, "feedback/enable").c_str(), channel->isEnabled() ? "1" : "0");
      } else if (sTopic.endsWith(String(channel->name) + F("/command/timer"))) {
        if (updateChannelTimerCommand(channel, payload, length)) {
          saveChannelsSettings();
        }
        std::string sTimer = std::to_string(channel->timer);
        _mqttClient.publish(getChannelTopic(channel, "feedback/timer").c_str(), sTimer.c_str());
      } else if (sTopic.endsWith(String(channel->name) + F("/command/rename"))) {
        if (renameChannelCommand(channel, payload, length)) {
          saveChannelsSettings();
        }
      } else if (sTopic.endsWith(String(channel->name) + F("/command/state"))) {
        // command/state topic is used to change the state on the channel with a desired value. So, receiving a mqtt
        // message with this purpose has sense only if the channel is an output one.
        if (channel->isEnabled() && channel->isOutput()) {
          changeOutputChannelStateCommand(channel, payload, length);
        }
        _mqttClient.publish(getChannelTopic(channel, "feedback/state").c_str(), channel->currState == LOW ? "1" : "0");
      }
    }
  }
  if (_mqttMessageCallback) {
    #ifdef LOGGING
    debug(F("Passing mqtt callback to user"));
    #endif
    // Workaround necesario porque el topic recibido desde mqtt se blanqueaba luego de un publish invocado dentro de la iteracion de los canales.
    _mqttMessageCallback(&sTopic[0], payload, length);
  }
}
#endif

bool ESPDomotic::changeOutputChannelStateCommand(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Processing command to change channel state"), channel->name);
  #endif
  if (length < 1) {
    #ifdef LOGGING
    debug(F("Invalid payload"));
    #endif
    return false;
  }
  int* value = reinterpret_cast<int*>(payload);
  return updateChannelState(channel, *value);
}

bool ESPDomotic::updateChannelState (Channel* channel, int state) {
  if (channel->currState == state) {
    #ifdef LOGGING
    debug(F("Channel is in same state, skipping"), state);
    #endif
    return false;
  } else {
    channel->write(state);
    return true;
  }
}

void ESPDomotic::lockBehaviour(bool lock) {
  this->_behaviourLocked = lock;
}

void ESPDomotic::moduleHardReset () {
  #ifdef LOGGING
  debug(F("Doing a module hard reset"));
  #endif
  LittleFS.format();
  WiFi.disconnect();
  delay(200);
  ESP.restart();
}

void ESPDomotic::moduleSoftReset () {
  #ifdef LOGGING
  debug(F("Doing a module soft reset"));
  #endif
  WiFi.disconnect();
  delay(200);
  ESP.restart();
}

bool ESPDomotic::enableChannelCommand(Channel* channel, unsigned char* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Updating channel enablement"), channel->name);
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

bool ESPDomotic::renameChannelCommand(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Processing command to update channel name"), channel->name);
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
    debug(F("New channel name"), newName);
    #endif
    #ifndef MQTT_OFF
    _mqttClient.unsubscribe(getChannelTopic(channel, "command/+").c_str());
    channel->updateName(newName);
    _mqttClient.subscribe(getChannelTopic(channel, "command/+").c_str());
    #endif
  }
  return renamed;
}

bool ESPDomotic::updateChannelTimerCommand(Channel* channel, uint8_t* payload, unsigned int length) {
  #ifdef LOGGING
  debug(F("Processing command to change channel timer"), channel->name);
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
  long newTimer = String(buff).toInt();
  #ifdef LOGGING
  debug(F("New timer in seconds"), newTimer);
  #endif
  bool timerChanged = channel->timer != (unsigned long) newTimer * 1000;
  channel->timer = newTimer * 1000; // received in seconds set in millis
  return timerChanged;
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
  if (_channelsCount < MAX_CHANNELS) {
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

#ifndef MQTT_OFF
String ESPDomotic::getChannelTopic (Channel *channel, String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + channel->name + F("/") + suffix;
  // std::string s2 = std::string(getModuleType()) + "/" + getModuleLocation() + "/" + getModuleName() + "/" + channel->name + "/" + suffix.c_str();
}

String ESPDomotic::getStationTopic (String suffix) {
  return String(getModuleType()) + F("/") + getModuleLocation() + F("/") + getModuleName() + F("/") + suffix;
}
#endif

bool ESPDomotic::loadConfig () {
  size_t size = getFileSize("/config.json");
  if (size > 0) {
    #ifdef USE_JSON
    char buff[size];
    loadFile("/config.json", buff, size);
    StaticJsonDocument<200> doc;
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
    File configFile = LittleFS.open("/config.json", "r");
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
  File file = LittleFS.open("/config.json", "w");
  if (file) {
    #ifdef USE_JSON
    StaticJsonDocument<200> doc;
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
  if (LittleFS.begin()) {
    if (LittleFS.exists(fileName)) {
      File file = LittleFS.open(fileName, "r");
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

void ESPDomotic::loadFile (const char* fileName, char* buff, size_t size) {
  if (LittleFS.begin()) {
    if (LittleFS.exists(fileName)) {
      File file = LittleFS.open(fileName, "r");
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

bool ESPDomotic::updateConf(const char* key, char* value) {
  File file = LittleFS.open(key, "w");
  #ifdef LOGGING
  debug("Updating conf with size", strlen(value));
  #endif
  if (file) {
    file.print(value);
    file.close();
    return true; 
  }
  return false;
}

char* ESPDomotic::getConf(const char* key) {
  size_t size = getFileSize(key);
  #ifdef LOGGING
  debug("Getting conf with size", size);
  #endif
  if (size > 0) {
    char* file = new char[size + 1];
    loadFile(key, file, size);
    file[size] = '\0'; // workaround para evitar cargar basura desde el fs.
    return file;
  }
  return NULL;
}

bool ESPDomotic::loadChannelsSettings () {
  if (_channelsCount > 0) {
    size_t size = getFileSize("/settings.json");
    if (size > 0) {
      #ifdef USE_JSON
      char buff[size];
      loadFile("/settings.json", buff, size);
      StaticJsonDocument<384> doc;
      DeserializationError error = deserializeJson(doc, buff);
      #ifdef LOGGING
      serializeJsonPretty(doc, Serial);
      #endif
      if (!error) {
        for (uint8_t i = 0; i < _channelsCount; ++i) {
          _channels[i]->updateName(doc[String(_channels[i]->id) + "_n"]);
          _channels[i]->timer = doc[String(_channels[i]->id) + "_t"];
          _channels[i]->enabled = doc[String(_channels[i]->id) + "_e"];
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
      File configFile = LittleFS.open("/settings.json", "r");
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
              if (key.endsWith("_n")) {
                _channels[i]->updateName(val.c_str());
              } else if (key.endsWith("_t")) {
                _channels[i]->timer = val.toInt();
              } else if (key.endsWith("_e")) {
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
  File file = LittleFS.open("/settings.json", "w");
  if (file) {
    #ifdef USE_JSON
    //TODO Trim param values
    StaticJsonDocument<384> doc;
    for (uint8_t i = 0; i < _channelsCount; ++i) {
      doc[String(_channels[i]->id) + "_n"] = _channels[i]->name;
      doc[String(_channels[i]->id) + "_t"] = _channels[i]->timer;
      doc[String(_channels[i]->id) + "_e"] = _channels[i]->enabled;
    }
    serializeJson(doc, file);
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    serializeJsonPretty(doc, Serial);
    #endif
    #else
    for (uint8_t i = 0; i > _channelsCount; ++i) {
      String line = String(_channels[i]->id) + "_n=" + String(_channels[i]->name);
      file.println(line);
      line = String(_channels[i]->id) + "_t=" + String(_channels[i]->timer);
      file.println(line);
      line = String(_channels[i]->id) + "_e=" + String(_channels[i]->enabled);
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

void ESPDomotic::setWifiConnectTimeout (uint16_t seconds) {
  _wifiConnectTimeout = seconds;
}

void ESPDomotic::setConfigPortalTimeout (uint16_t seconds) {
  _configPortalTimeout = seconds;
}

void ESPDomotic::setConfigFileSize (uint16_t bytes) {
  _configFileSize = bytes;
}

#ifdef LOGGING
template <class T> void ESPDomotic::debug (T text) {
  Serial.print("*DOMO: ");
  Serial.println(text);
  #ifdef MQTT_LOG
  #ifndef MQTT_OFF
  if (_mqttClient.connected()) {
    String s = String(ESP.getChipId()) + ": " + String(text);
    _mqttClient.publish("/domotic/log", s.c_str());
  }
  #endif
  #endif
}

template <class T, class U> void ESPDomotic::debug (T key, U value) {
  Serial.print("*DOMO: ");
  Serial.print(key);
  Serial.print(": ");
  Serial.println(value);
  #ifdef MQTT_LOG
  #ifndef MQTT_OFF
  if (_mqttClient.connected()) {
    String s = String(ESP.getChipId()) + ": " + String(key) + " " + String(value);
    _mqttClient.publish("/domotic/log", s.c_str());
  }
  #endif
  #endif
}
#endif

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, int currState) {
  init(id, name, pin, pinMode, currState, false, NO_TIMER);
}
Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, int currState, bool analog) {
  init(id, name, pin, pinMode, currState, analog, NO_TIMER);
}
Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, int currState, uint32_t timer) {
  init(id, name, pin, pinMode, currState, false, timer);
}
Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, int currState, bool analog, uint32_t timer) {
  init(id, name, pin, pinMode, currState, analog, timer);
}

void Channel::init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, int currState, bool analog, uint32_t timer) {
  this->id = id;
  this->pin = pin;
  this->currState = currState;
  this->analog = analog;
  this->timer = timer;
  this->enabled = true;
  this->pinMode = pinMode;
  this->name = new char[_channelNameMaxLength + 1];
  // This is because if it is an input channel we want timerControl not to be 0 in order to timer check works properly
  this->timerControl = pinMode == OUTPUT ? 0 : timer;
  updateName(name);
}

void Channel::updateName (const char *v) {
  String(v).toCharArray(this->name, _channelNameMaxLength);
}

void Channel::updateTimerControl() {
  this->timerControl = millis() + this->timer;
}

bool Channel::timeIsUp() {
  return this->timerControl > 0 && millis() > this->timerControl;
}

bool Channel::isEnabled () {
  return this->enabled && this->name != NULL && strlen(this->name) > 0;
}

bool Channel::isOutput() {
  return this->pinMode == OUTPUT;
}

void Channel::write(int value) {
  this->prevState = this->currState;
  if (this->analog) {
    this->currState = value;
    #ifdef LOGGING
    // debug(F("Changing channel state to"), this->currState);
    #endif
    analogWrite(this->pin, this->currState);
  } else {
    if (this->binary) {
      if (value == HIGH) {
        this->updateTimerControl();
      }
      if (this->inverted) {
        value = value == LOW ? HIGH : LOW;
      }
    } 
    this->currState = value;
    #ifdef LOGGING
    // debug(F("Changing channel state to"), this->currState == HIGH ? "[ON]" : "[OFF]");
    #endif   
    digitalWrite(this->pin, this->currState);
  }
}

bool Channel::read() {
  this->prevState = this->currState;
  if (this->timeIsUp()) {
    if (this->analog) {
      this->currState = analogRead(this->pin);
    } else {
      int read = digitalRead(this->pin);
      if (this->binary) {
        if (this->inverted) {
          read = read == LOW ? HIGH : LOW;
        }
      }
      this->currState = read;
    }
    this->updateTimerControl();
    return true;
  }
  return false;
}
