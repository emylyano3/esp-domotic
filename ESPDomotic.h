#ifndef ESPDomotic_h
#define ESPDomotic_h

#include <Arduino.h>
#include <PubSubClient.h>

#ifndef INVALID_PIN_NO
#define INVALID_PIN_NO 255
#endif

#ifndef MIN_SIGNAL_QUALITY
#define MIN_SIGNAL_QUALITY 30
#endif

#ifndef WIFI_CONNECT_TIMEOUT
#define WIFI_CONNECT_TIMEOUT 5000
#endif

#ifndef MQTT_BROKER_CONNECT_RETRY
#define MQTT_BROKER_CONNECT_RETRY 5000
#endif

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

        /* Conf getters */
        // Returns the mqtt host the user configured
        const char*     getMqttServerHost();
        // Returns the mqtt port the user configured
        uint16_t        getMqttServerPort();
        // Returns the name the user gave to the module during configuration
        const char*     getModuleName();
        // Returns the location name the user gave to the module during configuration
        const char*     getModuleLocation();
        // Returns the name wich the module is subscribed to the AP
        const char*     getStationName();
        // Returns the inner mqtt client
        PubSubClient*   getMqttClient();

        /* Utils */
        // Returns the size of a file
        size_t  getFileSize (const char* fileName);
        // Loads a file into the buffer.
        void    loadFile (const char* fileName, char buff[], size_t size);

    private:
        bool            _debug          = true;
        const char*     _moduleType     = "generic";
        const char*     _apSSID         = NULL;
        uint8_t         _feedbackPin    = INVALID_PIN_NO;

        /* Mqtt callbacks */
        std::function<void()>                               _mqttConnectionCallback;
        std::function<void(char*, uint8_t*, unsigned int)>  _mqttMessageCallback;
        
        /* Utils */
        void            connectBroker();
        bool            loadConfig();
        void            saveConfig();

        /* Logging */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif