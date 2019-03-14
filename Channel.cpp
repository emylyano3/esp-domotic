#include "Channel.hpp"

Property* Property::setId(const char* id) {
  this->_id = id;
  return this;
}

Property* Property::setName(const char* name) {
  this->_name = name;
  return this;
}

Property* Property::setDataType(const char* type) {
  this->_datatype = type;
  return this;
}

Property* Property::setUnit(const char* unit) {
  this->_unit = unit;
  return this;
}

Property* Property::setFormat(const char* format) {
  this->_format = format;
  return this;
}

Property* Property::setSettable(bool settable) {
  this->_settable = settable;
  return this;
}

Property* Property::setRetained(bool retained) {
  this->_retained = retained;
  return this;
}

const char* Property::getId(){return this->_id;}
const char* Property::getName(){return this->_name;}
const char* Property::getDataType(){return this->_datatype;}
const char* Property::getUnit(){return this->_unit;}
const char* Property::getFormat(){return this->_format;}
bool Property::isSettable(){return this->_settable;}
bool Property::isRetained(){return this->_retained;}


Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state) {
  init(id, name, pin, pinMode, state, -1);
}

Channel::Channel(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
  init(id, name, pin, pinMode, state, timer);
}

Channel::~Channel() {
  delete[] name;
  delete prop;
}

void Channel::init(const char* id, const char* name, uint8_t pin, uint8_t pinMode, uint8_t state, uint16_t timer) {
  this->id = id;
  this->pin = pin;
  this->state = state;
  this->timer = timer;
  this->enabled = true;
  this->pinMode = pinMode;
  this->name = new char[_channelNameMaxLength + 1];
  this->prop = new Property();
  updateName(name);
}

void Channel::updateName (const char *v) {
  String(v).toCharArray(this->name, _channelNameMaxLength);
}

void Channel::updateTimerControl() {
  this->timerControl = millis() + this->timer;
}

bool Channel::isEnabled () {
  return this->enabled && this->name != NULL && strlen(this->name) > 0;
}

Property* Channel::property () {
  return this->prop;
}