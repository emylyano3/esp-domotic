#ifndef MQTTModule_h
#define MQTTModule_h

#include <Arduino.h>

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

        /* Conf getters */
        uint16_t        getMqttServerPort();
        const char*     getMqttServerHost();
        const char*     getModuleName();
        const char*     getModuleLocation();
        
        /* Utils */
        void    loadFile (const char* fileName, char buff[], size_t size);
        size_t  getFileSize (const char* fileName);

    private:
        bool _debug = true;

        /* Utils */
        bool        loadConfig();
        void        saveConfig();
        char*       getStationName();

        /* Logging */
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif