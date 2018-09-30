#ifndef MQTTModule_h
#define MQTTModule_h

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

    private:
        bool _debug = true;

        /* Utils */
        void                                saveConfig();
        char*                               getStationName();
        template <class T> void             debug(T text);
        template <class T, class U> void    debug(T key, U value);
};
#endif