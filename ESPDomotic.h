#pragma once

#include <Arduino.h>

#ifndef MQTT_OFF
#include <PubSubClient.h>
#endif
#include <ESP8266WebServer.h>

const uint8_t       _invalidPinNo                 = 255;

#ifndef MQTT_OFF
const unsigned long _mqttBrokerReconnectionRetry  = 5  * 1000;
#endif
const uint8_t       _wifiMinSignalQuality         = 30;
const uint8_t       _channelNameMaxLength         = 20;
const uint8_t       _paramValueMaxLength          = 20;
const uint8_t       _paramIPValueLength           = 16;   // IP max length is 15 chars
const uint8_t       _paramPortValueLength         = 6;    // port range is from 0 to 65535

class Property {
    private:
        const char* _id;
        const char* _name;
        const char* _datatype;
        const char* _unit;
        const char* _format;
        bool        _settable = false;
        bool        _retained = true;

    public:
        Property() : _id(""), _name(""), _datatype(""), _unit(""), _format(""), _settable(false), _retained(true){}
        Property* setId(const char* id);
        Property* setName(const char* name);
        Property* setDataType(const char* type);
        Property* setUnit(const char* unit);
        Property* setFormat(const char* format);
        Property* setSettable(bool settable);
        Property* setRetained(bool retained);
        
        const char* getId();
        const char* getName();
        const char* getDataType();
        const char* getUnit();
        const char* getFormat();
        bool isSettable();
        bool isRetained();
};

class Channel {
    public:
        Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state);
        Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer);
        ~Channel();

        const char*     id;
        char*           name;
        uint8_t         pin;
        uint8_t         pinMode;
        uint8_t         state;
        unsigned long   timer;
        bool            enabled;
        Property*       prop;

        unsigned long   timerControl;
        
        void        init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer);

        // Updates the channelĂ‚Â´s name
        void        updateName (const char *v);
        // Updates the timer control setting it to timer time from now
        void        updateTimerControl();
        // Returns if the channel is enabled or not
        bool        isEnabled ();
        // Returns the property that the channel exposes
        Property*   property();
};

/*
Provides this functionality:
> HTTP update
> MQTT Client
> WIFI and module configuration 
> Configuration persistence & loading
*/
class ESPDomotic {
    public:
        ESPDomotic();
        ~ESPDomotic();

        /* Main methods */
        // Must be called when all setup is done
        void    init();
        // Must be called inside main loop
        void    loop();

        /* Module settings */
        // Sets the SSID for the configuration portal (When module enters in AP mode)
        void                setPortalSSID(const char* ssid);
        // Sets what type of module is it
        void                setModuleType(const char* mt);
        // Sets the pin through wich signal feedback is given to the user (a speaker, led, etc)
        void                setFeedbackPin(uint8_t fp);
        // Returns the name the user gave to the module during configuration
        const char*         getModuleName();
        // Returns the location name the user gave to the module during configuration
        const char*         getModuleLocation();
        // Returns the module type
        const char*         getModuleType();
        // Returns the name wich the module is subscribed to the AP
        const char*         getStationName();
        // Resets the module and erases persisted data & wifi settings (factory restore)
        void                moduleHardReset ();
        void                setWifiConnectTimeout (uint16_t seconds);
        void                setConfigPortalTimeout (uint16_t seconds);
        void                setConfigFileSize (uint16_t bytes);

        #ifndef MQTT_OFF
        /* MQTT */
        // Sets the callback to be called just after the connection to mqtt broker has been stablished
        void                setMqttConnectionCallback(std::function<void()> callback);
        // Sets the callback used to tell that a message was received via mqtt
        void                setMqttMessageCallback(std::function<void(char*, uint8_t*, unsigned int)> callback);
        // Returns the mqtt host the user configured
        const char*         getMqttServerHost();
        // Returns the mqtt port the user configured
        uint16_t            getMqttServerPort();
        // Returns the inner mqtt client
        PubSubClient*       getMqttClient();
        String              getStationTopic (String cmd);
        // Returns the mqtt topic to which a channel may be subscribed
        String              getChannelTopic (Channel *c, String cmd);

        // Homie functions
        void                setFirmwareName (const char *fwName);
        const char*         getFirmwareName ();
        void                setFirmwareVersion (const char *fwVersion);
        const char*         getFirmwareVersion ();
        void                setStatsInterval (uint8_t interval);
        uint8_t             getStatsInterval ();
        #endif

        /*HTTP Server*/
        ESP8266WebServer*   getHttpServer();

        /* Channels */
        // Returns the i'th  channel
        Channel         *getChannel(uint8_t i);
        // Get the quantity of channels configured
        uint8_t         getChannelsCount();
        // Save the channel settings in FS
        void            saveChannelsSettings ();
        // Sets the channel's state lo HIGH
        bool            openChannel (Channel* c);
        // Sets the channel's state lo LOW
        bool            closeChannel (Channel* c);
        // Adds new channel to manage
        void            addChannel(Channel* c);
        // To rename a channel
        bool            renameChannel(Channel* c, uint8_t* payload, unsigned int length);
        // To change the state of a channel. Intened to use with channel configures as OUTPUT
        bool            changeState(Channel* c, uint8_t* payload, unsigned int length);
        // To update the timer of a channel
        bool            updateChannelTimer(Channel* c, uint8_t* payload, unsigned int length);
        // To enable/disable a channel
        bool            enableChannel(Channel* c, unsigned char* payload, unsigned int length);

        /* Utils */
        // Returns the size of a file
        size_t          getFileSize (const char* fileName);
        // Loads a file into the buffer.
        void            loadFile (const char* fileName, char buff[], size_t size);

        /* Logging */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);

    private:
        const char*     _moduleType     = "generic";
        const char*     _deviceName     = "generic";
        const char*     _apSSID         = NULL;
        uint8_t         _feedbackPin    = _invalidPinNo;
        Channel**       _channels;

        #ifndef MQTT_OFF
        /* Mqtt callbacks */
        // Called after connection to mqtt broker has been stablished.
        // This lets user to suscribe to any topic he wants
        std::function<void()>                               _mqttConnectionCallback;
        // Called when an income mqtt message is received.
        // This lets user to process all incoming message to any topic it has suscribed.
        std::function<void(char*, uint8_t*, unsigned int)>  _mqttMessageCallback;
        /* Receives the message from the mqtt client */
        void            receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length);
        void            connectBroker();
        void            subscribe();
        void            homieSignUp();
        void            sendStats();
        String          toStringIp(IPAddress ip);
        #endif
        
        /* Utils */
        bool            loadConfig();
        void            saveConfig();
        bool            loadChannelsSettings();
};