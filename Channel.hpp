#pragma once

#include <Arduino.h>

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
    
    const uint8_t       _channelNameMaxLength         = 20;

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
        
        void    init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer);

        // Updates the channel´s name
        void    updateName (const char *v);
        // Updates the timer control setting it to timer time ftom now
        void    updateTimerControl();
        // Returns if the channel is enabled or not
        bool    isEnabled ();
        // Returns the property that the channel exposes
        Property*   property();
};