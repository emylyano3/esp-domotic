#ifndef MQTTModule_h
#define MQTTModule_h

#include <Arduino.h>
#include <PubSubClient.h>

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
        void    init();
        
        // Must be called inside main loop
        void    loop();

        void    setModuleType(const char* mt);
        
        /* Setting methods */
        void    setMqttConnectionCallback(std::function<void()> callback);
        void    setMqttMessageCallback(std::function<void(char*, uint8_t*, unsigned int)> callback);

        /* Conf getters */
        uint16_t        getMqttServerPort();
        const char*     getMqttServerHost();
        const char*     getModuleName();
        const char*     getModuleLocation();
        PubSubClient*   getMqttClient();

        /* Utils */
        void    loadFile (const char* fileName, char buff[], size_t size);
        size_t  getFileSize (const char* fileName);

    private:
        bool        _debug      = true;
        const char* _moduleType;

        /* Mqtt callbacks */
        std::function<void()>                               _mqttConnectionCallback;
        std::function<void(char*, uint8_t*, unsigned int)>  _mqttMessageCallback;
        
        /* Utils */
        void            connectBroker();
        bool            loadConfig();
        void            saveConfig();
        const char*     getStationName();

        /* Logging */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif