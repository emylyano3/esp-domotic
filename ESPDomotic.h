#ifndef ESPDomotic_h
#define ESPDomotic_h

#include <Arduino.h>
#include <PubSubClient.h>

const uint8_t       _invalidPinNo                 = 255;
const unsigned long _wifiConnectTimeout           = 5000;
const unsigned long _mqttBrokerReconnectionRetry  = 5000;
const uint8_t       _wifiMinSignalQuality         = 30;
const uint8_t       _channelNameMaxLength         = 20;
const uint8_t       _paramValueMaxLength          = 20;
const uint8_t       _paramIPValueLength           = 16;   // IP max length is 15 chars
const uint8_t       _paramPortValueLength         = 6;    // port range is from 0 to 65535
const char          _stateON                      = '1';
const char          _stateOFF                     = '0';

class Channel {
    public:
        Channel(const char* id, const char* name, uint8_t pin, uint8_t state, uint16_t timer, uint8_t pinMode);
        const char*     id;
        char*           name;
        uint8_t         pin;
        char            state;
        bool            enabled;
        unsigned long   timer;
        unsigned long   timerControl;
        uint8_t         pinMode;

        // void    setTimer (unsigned long t);
        // unsigned long     getTimer ();
        void    updateName (const char *v);
        // Updates the timer control setting it to timer time ftom now
        void    updateTimerControl();
        bool    isEnabled ();
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
        // Sets what type of module is it
        void    setModuleType(const char* mt);
        // Enables/Disables debugging
        void    setDebugOutput(bool debug);

        /* Setting methods */
        // Sets the callback to be called just after the connection to mqtt broker has been stablished
        void    setMqttConnectionCallback(std::function<void()> callback);
        // Sets the callback used to tell that a message was received via mqtt
        void    setMqttMessageCallback(std::function<void(char*, uint8_t*, unsigned int)> callback);
        // Sets the pin through wich signal feedback is given to the user (a speaker, led, etc)
        void    setFeedbackPin(uint8_t fp);
        // Sets the SSID for the configuration portal (When module enters in AP mode)
        void    setPortalSSID(const char* ssid);
        // Adds new channel to manage
        void    addChannel(Channel* c);

        /* Conf getters */
        // Returns the mqtt host the user configured
        const char*     getMqttServerHost();
        // Returns the mqtt port the user configured
        uint16_t        getMqttServerPort();
        // Returns the name the user gave to the module during configuration
        const char*     getModuleName();
        // Returns the location name the user gave to the module during configuration
        const char*     getModuleLocation();
        // Returns the module type
        const char*     getModuleType();
        // Returns the name wich the module is subscribed to the AP
        const char*     getStationName();
        // Returns the inner mqtt client
        PubSubClient*   getMqttClient();
        // Returns the i'th  channel
        Channel         *getChannel(uint8_t i);
        // Get the quantity of channels configured
        uint8_t         getChannelsCount();
        String          getStationTopic (String cmd);
        String          getChannelTopic (Channel *c, String cmd);

        /* Utils */
        // Returns the size of a file
        size_t  getFileSize (const char* fileName);
        // Loads a file into the buffer.
        void    loadFile (const char* fileName, char buff[], size_t size);
        // Save the channel settings in FS
        void    saveChannelsSettings();
        void    openChannel(Channel* c);
        void    closeChannel(Channel* c);
        void    moduleHardReset ();

        /* Logging */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);

    private:
        bool            _debug          = true;
        const char*     _moduleType     = "generic";
        const char*     _apSSID         = NULL;
        uint8_t         _feedbackPin    = _invalidPinNo;
        Channel**       _channels;

        /* Mqtt callbacks */
        // Called after connection to mqtt broker has been stablished.
        // This lets user to suscribe to any topic he wants
        std::function<void()>                               _mqttConnectionCallback;
        // Called when an income mqtt message is received.
        // This lets user to process all incoming message to any topic it has suscribed.
        std::function<void(char*, uint8_t*, unsigned int)>  _mqttMessageCallback;
        
        /* Utils */
        void            connectBroker();
        bool            loadConfig();
        void            saveConfig();
        bool            loadChannelsSettings();
        bool            renameChannel(Channel* c, uint8_t* payload, unsigned int length);
        bool            updateChannelTimer(Channel* c, uint8_t* payload, unsigned int length);
        bool            enableChannel(Channel* c, unsigned char* payload, unsigned int length);
        void            receiveMqttMessage(char* topic, uint8_t* payload, unsigned int length);
};
#endif