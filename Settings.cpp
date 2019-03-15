#include "Settings.hpp"

Settings::Settings (ESPConfigParam* h, ESPConfigParam* p, ESPConfigParam* n, ESPConfigParam* l, Channel** c, uint8_t cc) {
    _mqttHost = h;
    _mqttPort = p;
    _moduleName = n;
    _moduleLocation = l;
    _channels = c;
    _channelsCount = cc;
}

void Settings::loadFile (const char* fileName, char buff[], size_t size) {
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
size_t Settings::getFileSize (const char* fileName) {
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

bool Settings::loadConfig () {
  size_t size = getFileSize("/config.json");
  if (size > 0) {
    #ifndef ESP01
    char buff[size];
    loadFile("/config.json", buff, size);
    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, buff);
    if (!error) {
      #ifndef MQTT_OFF
      _mqttHost->updateValue(doc[_mqttHost->getName()]);
      _mqttPort->updateValue(doc[_mqttPort->getName()]);
      #endif
      _moduleLocation->updateValue(doc[_moduleLocation->getName()]);
      _moduleName->updateValue(doc[_moduleName->getName()]);
      #ifdef LOGGING
      serializeJsonPretty(doc, Serial);
      Serial.println();
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
void Settings::saveConfig () {
  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    #ifndef ESP01
    DynamicJsonDocument doc(200);
    //TODO Trim param values
    #ifndef MQTT_OFF
    doc[_mqttHost->getName()] = _mqttHost->getValue();
    doc[_mqttPort->getName()] = _mqttPort->getValue();
    #endif
    doc[_moduleLocation->getName()] = _moduleLocation->getValue();
    doc[_moduleName->getName()] = _moduleName->getValue();
    serializeJson(doc, file);
    #ifdef LOGGING
    debug(F("Configuration file saved"));
    serializeJsonPretty(doc, Serial);
    Serial.println();
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
    debug("Failed to open config file for writing");
    #endif
  }
}

bool Settings::loadChannelsSettings () {
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
      Serial.println();
      #endif
      if (!error) {
        for (uint8_t i = 0; i < _channelsCount; ++i) {
          _channels[i]->updateName(doc[String(_channels[i]->id) + "_name"]);
          _channels[i]->timer = doc[String(_channels[i]->id) + "_timer"];
          _channels[i]->enabled = doc[String(_channels[i]->id) + "_enabled"];
          #ifdef LOGGING
          serializeJsonPretty(doc, Serial);
          Serial.println();
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
    debug("No channel configured");
    #endif
    return false;
  }
}

void Settings::format () {
    SPIFFS.format();
}

void Settings::saveChannelsSettings () {
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
    Serial.println();
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