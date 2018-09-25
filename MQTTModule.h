#ifndef MQTTModule_h
#define MQTTModule_h

#include <PubSubClient.h>
#include <ESPConfig.h>

#define STATION_NAME_LENGTH 50

/*
Provides this functionality:
> HTTP update
> MQTT Client
> WIFI and module configuration 
> Configuration persistence & loading
*/
class MQTTModule {
    public:
        MQTTModule();
        ~MQTTModule();

        /* Main methods */
        // Must be called when all setup is done
        void init();
        
        // Must be called inside main loop
        void loop();

        /* Getters */
        ESPConfig*      getConfig();
        PubSubClient*   getMQTTClient();
        char*           getStationName();

        /* Setup methods */
        void            setDebugOutput(bool debug);
        void            setModuleType(String mt);
        void            setFeedbackPin(uint8_t pin);
        void            setSubscriptionCallback(void(*callback)(void));
        void            setMqttReceiveCallback(void (*callback)(char*, uint8_t*, unsigned int));

        /* Utils */
        size_t          getFileSize (const char* fileName);
        void            loadFile (const char* fileName, char buff[], size_t size);

    private:
        /* State */
        bool            _debug;
        char            _stationName[STATION_NAME_LENGTH];
        String          _moduleType = "generic";
        uint8_t         _feedbackPin;

        /* Callbacks */
        void            (*_subscriptionCallback)(void)                      = NULL;
        void            (*_mqttCallback)(char*, uint8_t*, unsigned int)     = NULL;

        /* Utils */
        bool                                loadConfig ();
        void                                connectBroker();
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif