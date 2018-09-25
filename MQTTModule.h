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

    private:
        /* State */
        bool            _debug;
        char            _stationName[STATION_NAME_LENGTH];
        String          _moduleType = "generic";
        uint8_t         _feedbackPin;

        /* Callbacks */
        void            (*_subscriptionCallback)(void)      = NULL;

        /* Utils */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif