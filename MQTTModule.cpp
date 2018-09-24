#include "MQTTModule.h"
#include <WifiClient.h>
#include <PubSubClient.h>

ESPConfigParam::ESPConfigParam (InputType type, const char* name, const char* label, const char* defVal, uint8_t length, const char* html) {
  _type = type;
  _name = name;
  _label = label;
  _customHTML = html;
  _length = length;
  _value = new char[length + 1];
  updateValue(defVal);
}

ESPConfigParam::~ESPConfigParam() {
  if (_value != NULL) {
    delete[] _value;
  }
}

InputType ESPConfigParam::getType() {
  return _type;
}

const char* ESPConfigParam::getName() {
  return _name;
}

const char* ESPConfigParam::getValue() {
  return _value;
}

const char* ESPConfigParam::getLabel() {
  return _label;
}

int ESPConfigParam::getValueLength() {
  return _length;
}

const char* ESPConfigParam::getCustomHTML() {
  return _customHTML;
}

std::vector<char*> ESPConfigParam::getOptions() {
  return _options;
}

void ESPConfigParam::updateValue (const char *v) {
  String s = String(v);
  s.toCharArray(_value, _length);
}

WiFiClient      _wifiClient;
PubSubClient    _mqttClient(_wifiClient);

ESPConfigParam _mqttPortCfg (Text, "mqttPort", "MQTT port", "", 6, "required");  // port range is from 0 to 65535
ESPConfigParam _mqttHostCfg (Text, "mqttHost", "MQTT host", "", 16, "required"); // IP max length is 15 chars

void MQTTModule::setServer(const char* host, uint16_t port) {
  _mqttClient.setServer(host, port);
}

void MQTTModule::setCallback( void (*callback)(char* topic, uint8_t* payload, unsigned int length)) {
  _mqttClient.setCallback(callback);
}

MQTTModule::MQTTModule() {
  _max_params = ESP_CONFIG_MAX_PARAMS;
  _configParams = (ESPConfigParam**)malloc(_max_params * sizeof(ESPConfigParam*));
  _apName = String(ESP.getChipId()).c_str();
}

MQTTModule::~MQTTModule() {
  if (_configParams != NULL) {
    debug(F("Freeing allocated params!"));
    free(_configParams);
  }
}

void MQTTModule::connectWifiNetwork (bool existsConfig) {
  debug(F("Connecting to wifi network"));
  bool connected = false;
  while (!connected) {
    if (existsConfig) {
      debug(F("Connecting to saved network"));
      if (connectWiFi() == WL_CONNECTED) {
        connected = true;
      } else {
        debug(F("Could not connect to saved network. Going into config mode."));
        connected = startConfigPortal();
      }
    } else {
      debug(F("Going into config mode cause no config was found"));
      connected = startConfigPortal();
    }
  }
}

bool MQTTModule::startConfigPortal() {
  WiFi.mode(WIFI_AP_STA);
  _connect = false;
  setupConfigPortal();
  while(1) {
    _dnsServer->processNextRequest();
    _server->handleClient();
    if (_connect) {
      _connect = false;
      delay(1000);
      debug(F("Connecting to new AP"));
      // using user-provided  _ssid, _pass in place of system-stored ssid and pass
      //end the led feedback
      if (_feedbackPin != INVALID_PIN_NO) {
        //stop feedback
        digitalWrite(_feedbackPin, LOW);
      }
      if (connectWifi(_server->arg("s").c_str(), _server->arg("p").c_str()) != WL_CONNECTED) {
        debug(F("Failed to connect."));
        break;
      } else {
        WiFi.mode(WIFI_STA);
        //notify that configuration has changed and any optional parameters should be saved
        if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
        }
        break;
      }
    }
    if (_feedbackPin != INVALID_PIN_NO) {
      nonBlockingFeedback(_feedbackPin, 1000);
    }
    yield();
  }
  _server.reset();
  _dnsServer.reset();
  return  WiFi.status() == WL_CONNECTED;
}

void MQTTModule::setConnectionTimeout(unsigned long seconds) {
  _connectionTimeout = seconds;
}

void MQTTModule::setPortalSSID(const char *apName) {
  _apName = apName;
}

void MQTTModule::setPortalPassword(const char *apPass) {
  _apPass = apPass;
}

void MQTTModule::setMinimumSignalQuality(int quality) {
  _minimumQuality = quality;
}

void MQTTModule::setDebugOutput(bool debug) {
  _debug = debug;
}

void MQTTModule::setAPStaticIP(IPAddress ip, IPAddress gw, IPAddress sn) {
  _ap_static_ip = ip;
  _ap_static_gw = gw;
  _ap_static_sn = sn;
}

void MQTTModule::setFeedbackPin(uint8_t pin) {
  _feedbackPin = pin;
}

void MQTTModule::setAPCallback( void (*func)(MQTTModule* myESPConfig) ) {
  _apcallback = func;
}

void MQTTModule::setSaveConfigCallback( void (*func)(void) ) {
  _savecallback = func;
}

void MQTTModule::setStationNameCallback(char* (*func)(void)) {
  _getStationNameCallback = func;
}

ESPConfigParam* MQTTModule::getParameter(uint8_t index) {
  if (index >= _paramsCount) {
    return NULL;
  } else {
    return _configParams[index];
  }
}

bool MQTTModule::addParameter(ESPConfigParam *p) {
  if (_paramsCount + 1 > _max_params) {
    // rezise the params array
    _max_params += ESP_CONFIG_MAX_PARAMS;
    debug(F("Increasing _max_params to:"), _max_params);
    ESPConfigParam** newParams = (ESPConfigParam**)realloc(_configParams, _max_params * sizeof(ESPConfigParam*));
    if (newParams != NULL) {
      _configParams = newParams;
    } else {
      debug(F("ERROR: failed to realloc params, size not increased!"));
      return false;
    }
  }
  _configParams[_paramsCount] = p;
  _paramsCount++;
  debug(F("Adding parameter"), p->getName());
  return true;
}

void MQTTModule::blockingFeedback (uint8_t pin, long stepTime, uint8_t times) {
  for (uint8_t i = 0; i < times; ++i) {
    digitalWrite(pin, HIGH);
    delay(stepTime);
    digitalWrite(pin, LOW);
    delay(stepTime);
  }
}

void MQTTModule::nonBlockingFeedback(uint8_t pin, int stepTime) {
  if (millis() > _sigfbkStepControl + stepTime) {
    _sigfbkIsOn = !_sigfbkIsOn;
    _sigfbkStepControl = millis();
    digitalWrite(pin, _sigfbkIsOn ? HIGH : LOW);
  }
}

uint8_t MQTTModule::connectWifi(String ssid, String pass) {
  debug(F("Connecting as wifi client..."));
  if (WiFi.status() == WL_CONNECTED) {
    debug(F("Already connected. Bailing out."));
    return WL_CONNECTED;
  }
  if (_getStationNameCallback) {
    WiFi.hostname(_getStationNameCallback());
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  return waitForConnectResult();
}

uint8_t MQTTModule::connectWiFi() {
  WiFi.mode(WIFI_STA);
  if (_getStationNameCallback) {
    WiFi.hostname(_getStationNameCallback());
  }
  if (WiFi.SSID()) {
    debug(F("Using last saved values, should be faster"));
    //trying to fix connection in progress hanging
    ETS_UART_INTR_DISABLE();
    wifi_station_disconnect();
    ETS_UART_INTR_ENABLE();
    WiFi.begin();
    return waitForConnectResult();
  } else {
    debug(F("No saved credentials"));
    return WL_CONNECT_FAILED;
  }
}

uint8_t MQTTModule::waitForConnectResult() {
  if (_connectionTimeout == 0) {
    return WiFi.waitForConnectResult();
  } else {
    debug(F("Waiting for connection result with time out"));
    unsigned long start = millis();
    bool keepConnecting = true;
    uint8_t status, retry = 0;
    while (keepConnecting) {
      status = WiFi.status();
      if (millis() > start + _connectionTimeout) {
        keepConnecting = false;
        debug(F("Connection timed out"));
      }
      if (status == WL_CONNECTED) {
        keepConnecting = false;
      } else if (status == WL_CONNECT_FAILED) {
        debug(F("Connection failed. Retrying: "), ++retry);
        debug("Trying to begin connection again");
        WiFi.begin();
      }
      delay(100);
    }
    return status;
  }
}

void MQTTModule::setupConfigPortal() {
  _server.reset(new ESP8266WebServer(80));
  _dnsServer.reset(new DNSServer());
  debug(F("Configuring access point... "), _apName);
  if (_apPass != NULL) {
    if (strlen(_apPass) < 8 || strlen(_apPass) > 63) {
      debug(F("Invalid AccessPoint password. Ignoring"));
      _apPass = NULL;
    }
    debug(_apPass);
  }
  if (_ap_static_ip) {
    debug(F("Custom AP IP/GW/Subnet"));
    WiFi.softAPConfig(_ap_static_ip, _ap_static_gw, _ap_static_sn);
  }
  if (_apPass != NULL) {
    WiFi.softAP(_apName, _apPass);
  } else {
    WiFi.softAP(_apName);
  }
  // Without delay I've seen the IP address blank
  delay(500); 
  debug(F("AP IP address"), WiFi.softAPIP());
  /* Setup the DNS server redirecting all the domains to the apIP */
  _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  _dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
  /* Setup web pages */
  _server->on("/", std::bind(&MQTTModule::handleWifi, this, false));
  _server->on("/config", std::bind(&MQTTModule::handleWifi, this, false));
  _server->on("/scan", std::bind(&MQTTModule::handleWifi, this, true)); 
  _server->on("/wifisave", std::bind(&MQTTModule::handleWifiSave, this));
  _server->onNotFound(std::bind(&MQTTModule::handleNotFound, this));
  _server->begin();
  debug(F("HTTP server started"));
}

void MQTTModule::handleWifi(bool scan) {
  // If captive portal redirect instead of displaying the page.
  if (captivePortal()) { 
    return;
  }
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Proeza Domotics");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += "<h2>Module config</h2>";
  page += FPSTR(HTTP_HEAD_END);
  if (scan) {
    int n = WiFi.scanNetworks();
    debug(F("Scan done"));
    if (n == 0) {
      debug(F("No networks found"));
      page += F("No networks found. Refresh to scan again.");
    } else {
      //sort networks
      int indices[n];
      for (int i = 0; i < n; i++) {
        indices[i] = i;
      }
      // old sort
      for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
          if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            std::swap(indices[i], indices[j]);
          }
        }
      }
      // remove duplicates ( must be RSSI sorted )
      String cssid;
      for (int i = 0; i < n; i++) {
        if (indices[i] == -1) continue;
        cssid = WiFi.SSID(indices[i]);
        for (int j = i + 1; j < n; j++) {
          if (cssid == WiFi.SSID(indices[j])) {
            debug("DUP AP: " + WiFi.SSID(indices[j]));
            indices[j] = -1; // set dup aps to index -1
          }
        }
      }
      //display networks in page
      for (int i = 0; i < n; i++) {
        if (indices[i] == -1) continue; // skip dups
        debug(WiFi.SSID(indices[i]));
        debug(WiFi.RSSI(indices[i]));
        int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));
        if (_minimumQuality == -1 || _minimumQuality < quality) {
          String item = FPSTR(HTTP_ITEM);
          String rssiQ;
          rssiQ += quality;
          item.replace("{v}", WiFi.SSID(indices[i]));
          item.replace("{r}", rssiQ);
          if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE) {
            item.replace("{i}", "l");
          } else {
            item.replace("{i}", "");
          }
          page += item;
        } else {
          debug(F("Skipping due to quality"));
        }
      }
      page += "<br/>";
    }
  }
  page += FPSTR(HTTP_FORM_START);
  char parLength[5];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++) {
    if (_configParams[i]->getName() != NULL) {
      if (_configParams[i]->getType() == Combo) {
        String pitem = FPSTR(HTTP_FORM_INPUT_LIST);
        pitem.replace("{i}", _configParams[i]->getName());
        pitem.replace("{n}", _configParams[i]->getName());
        String ops = "";
        for (size_t j = 0; j < _configParams[i]->getOptions().size(); ++j) {
          String op = FPSTR(HTTP_FORM_INPUT_LIST_OPTION);
          op.replace("{o}", _configParams[i]->getOptions()[j]);
          ops.concat(op);
        }
        pitem.replace("{p}", _configParams[i]->getLabel());
        pitem.replace("{o}", ops);
        pitem.replace("{c}", _configParams[i]->getCustomHTML());
        page += pitem;
      } else {
        String pitem = FPSTR(HTTP_FORM_INPUT);
        pitem.replace("{i}", _configParams[i]->getName());
        pitem.replace("{n}", _configParams[i]->getName());
        pitem.replace("{p}", _configParams[i]->getLabel());
        snprintf(parLength, 5, "%d", _configParams[i]->getValueLength());
        pitem.replace("{l}", parLength);
        pitem.replace("{v}", _configParams[i]->getValue());
        pitem.replace("{c}", _configParams[i]->getCustomHTML());
        page += pitem;
      }
    } 
  }
  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_SCAN_LINK);
  page += FPSTR(HTTP_END);
  _server->sendHeader("Content-Length", String(page.length()));
  _server->send(200, "text/html", page);
  debug(F("Sent config page"));
}

void MQTTModule::handleNotFound() {
  // If captive portal redirect instead of displaying the error page.
  if (captivePortal()) { 
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += _server->uri();
  message += "\nMethod: ";
  message += _server->method() == HTTP_GET ? "GET" : "POST";
  message += "\nArguments: ";
  message += _server->args();
  message += "\n";
  for (int i = 0; i < _server->args(); i++) {
    message += " " + _server->argName(i) + ": " + _server->arg(i) + "\n";
  }
  _server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _server->sendHeader("Pragma", "no-cache");
  _server->sendHeader("Expires", "-1");
  _server->sendHeader("Content-Length", String(message.length()));
  _server->send(404, "text/plain", message);
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void MQTTModule::handleWifiSave() {
  for (int i = 0; i < _paramsCount; i++) {
    _configParams[i]->updateValue(_server->arg(_configParams[i]->getName()).c_str());
    debug(_configParams[i]->getName(), _configParams[i]->getValue());
  }
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Credentials Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += "<h2>Module config</h2>";
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_SAVED);
  page += FPSTR(HTTP_END);
  _server->sendHeader("Content-Length", String(page.length()));
  _server->send(200, "text/html", page);
  _connect = true; //signal ready to connect/reset
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
bool MQTTModule::captivePortal() {
  if (!isIp(_server->hostHeader()) ) {
    debug(F("Request redirected to captive portal"));
    _server->sendHeader("Location", String("http://") + toStringIp(_server->client().localIP()), true);
    _server->send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    _server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

bool MQTTModule::isIp(String str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String MQTTModule::toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

int MQTTModule::getRSSIasQuality(int RSSI) {
  int quality = 0;
  if (RSSI <= -100) {
    quality = 0;
  } else if (RSSI >= -50) {
    quality = 100;
  } else {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

template <class T> void MQTTModule::debug (T text) {
  if (_debug) {
    Serial.print("*ESPC: ");
    Serial.println(text);
  }
}

template <class T, class U> void MQTTModule::debug (T key, U value) {
  if (_debug) {
    Serial.print("*ESPC: ");
    Serial.print(key);
    Serial.print(": ");
    Serial.println(value);
  }
}