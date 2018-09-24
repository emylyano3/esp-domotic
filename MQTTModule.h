#ifndef MQTTModule_h
#define MQTTModule_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <memory>

extern "C" {
  #include "user_interface.h"
}

const char HTTP_HEAD[] PROGMEM                      = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
const char HTTP_STYLE[] PROGMEM                     = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;margin-top:3px;margin-bottom:3px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;margin-top:3px;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}</style>";
const char HTTP_SCRIPT[] PROGMEM                    = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_END[] PROGMEM                  = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char HTTP_ITEM[] PROGMEM                      = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char HTTP_FORM_START[] PROGMEM                = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='SSID' required><br/><input id='p' name='p' length=64 type='password' placeholder='password' required><hr/>";
const char HTTP_FORM_INPUT[] PROGMEM                = "<input id='{i}' name='{n}' placeholder='{p}' maxlength={l} value='{v}' {c}><br/>";
const char HTTP_FORM_INPUT_LIST[] PROGMEM           = "<input id='{i}' name='{n}' placeholder='{p}' list='d' {c}><datalist id='d'{o}></datalist><br/>";
const char HTTP_FORM_INPUT_LIST_OPTION[] PROGMEM    = "<option>{o}</option>";
const char HTTP_FORM_END[] PROGMEM                  = "<hr/><button type='submit'>Save</button></form>";
const char HTTP_SCAN_LINK[] PROGMEM                 = "<br/><div class=\"c\"><a href=\"/scan\">Scan for networks</a></div>";
const char HTTP_SAVED[] PROGMEM                     = "<div>Credentials Saved<br/>Trying to connect ESP to network.<br/>If it fails reconnect to AP to try again</div>";
const char HTTP_END[] PROGMEM                       = "</div></body></html>";

#ifndef INVALID_PIN_NO
#define INVALID_PIN_NO 127
#endif
#ifndef ESP_CONFIG_MAX_PARAMS
#define ESP_CONFIG_MAX_PARAMS 10
#endif

enum InputType {Combo, Text};

class ESPConfigParam {

    public:
        ESPConfigParam();
        ESPConfigParam (InputType type, const char* name, const char* label, const char* defVal, uint8_t length, const char* html);
        ~ESPConfigParam();

        InputType           getType();
        const char*         getName();
        const char*         getLabel();
        const char*         getValue();
        int                 getValueLength();
        const char*         getCustomHTML();
        std::vector<char*>  getOptions();

        void                updateValue(const char *v);

    private:
        const char*         _name;       // identificador
        const char*         _label;      // legible por usuario
        char*               _value;      // valor default
        uint8_t             _length;     // longitud limite
        const char*         _customHTML; // html custom
        InputType           _type;       // tipo de control en formularion
        std::vector<char*>  _options;    // optciones para el combo
};


class MQTTModule {
    public:
        MQTTModule();
        ~MQTTModule();

        void            setServer(const char* host, uint16_t port);
        void            setCallback(void (*callback)(char*, uint8_t*, unsigned int));
        void            connectWifiNetwork(bool existConfig);
        bool            startConfigPortal();

        /* setup methods */
        void            setConnectionTimeout(unsigned long seconds);
        void            setPortalSSID(const char *apName);
        void            setPortalPassword(const char *apPass);
        bool            addParameter(ESPConfigParam *p);
        void            setDebugOutput(bool debug);
        void            setFeedbackPin(uint8_t pin);
        void            setAPStaticIP(IPAddress ip, IPAddress gw, IPAddress sn);
        
        // Returns the param under the specified index
        ESPConfigParam *getParameter(uint8_t index);

        //called when AP mode and config portal is started
        void    setAPCallback( void (*func)(MQTTModule*) );
        
        //called when connecting station to AP
        void    setStationNameCallback(char* (*func)(void));
        
        //called when settings have been changed and connection was successful
        void    setSaveConfigCallback( void (*func)(void) );
        
        //defaults to not showing anything under 8% signal quality if called
        void    setMinimumSignalQuality(int quality = 8);

        // Blocking signal feedback. Turns on/off a signal a specific times waiting a step time for each state flip.
        void    blockingFeedback (uint8_t pin, long stepTime, uint8_t times);

        // Non blocking signal feedback (to be used inside a loop). Uses global variables to control when to flip the signal state according to the step time.
        void    nonBlockingFeedback(uint8_t pin, int stepTime);

    private:

        std::unique_ptr<DNSServer>        _dnsServer;
        std::unique_ptr<ESP8266WebServer> _server;
        
        const char*     _apName             = "ESP-Module";
        const char*     _apPass             = NULL;
        int             _minimumQuality     = -1;
        uint8_t         _paramsCount;
        uint8_t         _max_params;
        bool            _connect;
        bool            _debug              = true;
        uint8_t         _feedbackPin        = INVALID_PIN_NO;

        const uint8_t   DNS_PORT            = 53;

        uint8_t connectWifi(String ssid, String pass);
        uint8_t connectWiFi();
        uint8_t waitForConnectResult();
        void    setupConfigPortal();

        unsigned long       _connectionTimeout;
        
        // Signal feedback
        bool                _sigfbkIsOn           = false;
        unsigned long       _sigfbkStepControl    = 0;
        ESPConfigParam**    _configParams;
        
        IPAddress           _ap_static_ip;
        IPAddress           _ap_static_gw;
        IPAddress           _ap_static_sn;

        /* Callbacks */
        void        (*_apcallback)(MQTTModule*)         = NULL;
        void        (*_savecallback)(void)              = NULL;
        char*       (*_getStationNameCallback)(void)    = NULL;
        
        void        handleRoot();
        void        handleWifi(bool scan);
        void        handleWifiSave();
        void        handleInfo();
        void        handleReset();
        void        handleNotFound();
        void        handle204();
        bool        captivePortal();
        bool        configPortalHasTimeout();
        bool        isIp(String str);
        String      toStringIp(IPAddress ip);
        int         getRSSIasQuality(int RSSI);

        template <class T> void debug(T text);
        template <class T, class U> void debug(T key, U value);
};
#endif