#pragma once

#include <FS.h>
#include <Arduino.h>
#include <ESPConfig.h>
#include <ArduinoJson.h>
#include "Channel.hpp"
#include "Logger.hpp"

class Settings {
    private:
        #ifndef MQTT_OFF
        ESPConfigParam*  _mqttPort;
        ESPConfigParam*  _mqttHost;
        #endif
        ESPConfigParam*  _moduleName;
        ESPConfigParam*  _moduleLocation;
        Channel**       _channels;
        uint8_t         _channelsCount;

    public:
        Settings(ESPConfigParam* h, ESPConfigParam* p, ESPConfigParam* n, ESPConfigParam* l, Channel** c, uint8_t cc);
        void    loadFile (const char* fileName, char buff[], size_t size);
        size_t  getFileSize (const char* fileName);
        bool    loadConfig ();
        void    saveConfig ();
        bool    loadChannelsSettings ();
        void    saveChannelsSettings ();
        void    format ();
};