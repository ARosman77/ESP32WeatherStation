/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2017 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Modified by ARosman77 to use it in ESP32 project.
 * 
 */

#include "MySensorsLib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MyMessage::MyMessage()
{
  clear();
}

MyMessage::MyMessage(uint8_t _sensor, uint8_t _type)
{
  clear();
  sensor = _sensor;
  type   = _type;
}
/*
  uint8_t version_length;    
  // 2 bit - Protocol version
  // 1 bit - Signed flag
  // 5 bit - Length of payload
  uint8_t command_ack_payload;
  // 3 bit - Command type
  // 1 bit - Request an ack - Indicator that receiver should send an ack back.
  // 1 bit - Is ack message - Indicator that this is the actual ack message.
  // 3 bit - Payload data type
  uint8_t type;              // 8 bit - Type varies depending on command
  uint8_t sensor;            // 8 bit - Id of sensor that this message concerns.

  // Each message can transfer a payload. We add one extra byte for string
  // terminator \0 to be "printable" this is not transferred OTA
  // This union is used to simplify the construction of the binary data types transferred.
  union {
    uint8_t bValue;
    uint16_t uiValue;
    int16_t iValue;
    uint32_t ulValue;
    int32_t lValue;
    struct { // Float messages
      float fValue;
      uint8_t fPrecision;   // Number of decimals when serializing
    };
    struct {  // Presentation messages
      uint8_t version;    // Library version
      uint8_t sensorType;   // Sensor type hint for controller, see table above
    };
    char data[MAX_PAYLOAD + 1];
  } __attribute__((packed));
#ifdef __cplusplus
} __attribute__((packed));
#else
 */

// whole message without payload
MyMessage::MyMessage(uint8_t _node_id, uint8_t _sensor_id, uint8_t _command, bool ack, uint8_t _type)
{
  clear();
  sender = _node_id;
  sensor = _sensor_id;
  miSetCommand(_command);
  miSetAck(ack);
  type   = _type;
  miSetPayloadType(P_STRING);
}

void MyMessage::clear()
{
  last                = 0u;
  sender              = 0u;
  destination         = 0u;       // Gateway is default destination
  version_length      = 0u;
  command_ack_payload = 0u;
  type                = 0u;
  sensor              = 0u;
  (void)memset(data, 0u, sizeof(data));

  // set message protocol version
  miSetVersion(PROTOCOL_VERSION);
}

bool MyMessage::isAck() const
{
  return miGetAck();
}

uint8_t MyMessage::getCommand() const
{
  return miGetCommand();
}

/* Getters for payload converted to desired form */
void* MyMessage::getCustom() const
{
  return (void *)data;
}

const char* MyMessage::getString() const
{
  uint8_t payloadType = miGetPayloadType();
  if (payloadType == P_STRING) {
    return data;
  } else {
    return NULL;
  }
}

// handles single character hex (0 - 15)
char MyMessage::i2h(uint8_t i) const
{
  uint8_t k = i & 0x0F;
  if (k <= 9) {
    return '0' + k;
  } else {
    return 'A' + k - 10;
  }
}

char* MyMessage::getCustomString(char *buffer) const
{
  for (uint8_t i = 0; i < miGetLength(); i++) {
    buffer[i * 2] = i2h(data[i] >> 4);
    buffer[(i * 2) + 1] = i2h(data[i]);
  }
  buffer[miGetLength() * 2] = '\0';
  return buffer;
}

char* MyMessage::getStream(char *buffer) const
{
  uint8_t cmd = miGetCommand();
  if ((cmd == C_STREAM) && (buffer != NULL)) {
    return getCustomString(buffer);
  } else {
    return NULL;
  }
}

char* MyMessage::getString(char *buffer) const
{
  uint8_t payloadType = miGetPayloadType();
  if (buffer != NULL) {
    if (payloadType == P_STRING) {
      strncpy(buffer, data, miGetLength());
      buffer[miGetLength()] = 0;
    } else if (payloadType == P_BYTE) {
      itoa(bValue, buffer, 10);
    } else if (payloadType == P_INT16) {
      itoa(iValue, buffer, 10);
    } else if (payloadType == P_UINT16) {
      utoa(uiValue, buffer, 10);
    } else if (payloadType == P_LONG32) {
      ltoa(lValue, buffer, 10);
    } else if (payloadType == P_ULONG32) {
      ultoa(ulValue, buffer, 10);
    } else if (payloadType == P_FLOAT32) {
      dtostrf(fValue,2,min(fPrecision, (uint8_t)8),buffer);
    } else if (payloadType == P_CUSTOM) {
      return getCustomString(buffer);
    }
    return buffer;
  } else {
    return NULL;
  }
}

bool MyMessage::getBool() const
{
  return getByte();
}

uint8_t MyMessage::getByte() const
{
  if (miGetPayloadType() == P_BYTE) {
    return data[0];
  } else if (miGetPayloadType() == P_STRING) {
    return atoi(data);
  } else {
    return 0;
  }
}


float MyMessage::getFloat() const
{
  if (miGetPayloadType() == P_FLOAT32) {
    return fValue;
  } else if (miGetPayloadType() == P_STRING) {
    return atof(data);
  } else {
    return 0;
  }
}

int32_t MyMessage::getLong() const
{
  if (miGetPayloadType() == P_LONG32) {
    return lValue;
  } else if (miGetPayloadType() == P_STRING) {
    return atol(data);
  } else {
    return 0;
  }
}

uint32_t MyMessage::getULong() const
{
  if (miGetPayloadType() == P_ULONG32) {
    return ulValue;
  } else if (miGetPayloadType() == P_STRING) {
    return atol(data);
  } else {
    return 0;
  }
}

int16_t MyMessage::getInt() const
{
  if (miGetPayloadType() == P_INT16) {
    return iValue;
  } else if (miGetPayloadType() == P_STRING) {
    return atoi(data);
  } else {
    return 0;
  }
}

uint16_t MyMessage::getUInt() const
{
  if (miGetPayloadType() == P_UINT16) {
    return uiValue;
  } else if (miGetPayloadType() == P_STRING) {
    return atoi(data);
  } else {
    return 0;
  }

}

MyMessage& MyMessage::setType(uint8_t _type)
{
  type = _type;
  return *this;
}

MyMessage& MyMessage::setSensor(uint8_t _sensor)
{
  sensor = _sensor;
  return *this;
}

MyMessage& MyMessage::setDestination(uint8_t _destination)
{
  destination = _destination;
  return *this;
}

// Set payload
MyMessage& MyMessage::set(void* value, uint8_t length)
{
  uint8_t payloadLength = value == NULL ? 0 : min(length, (uint8_t)MAX_PAYLOAD);
  miSetLength(payloadLength);
  miSetPayloadType(P_CUSTOM);
  memcpy(data, value, payloadLength);
  return *this;
}

MyMessage& MyMessage::set(const char* value)
{
  uint8_t length = value == NULL ? 0 : min(strlen(value), (size_t)MAX_PAYLOAD);
  miSetLength(length);
  miSetPayloadType(P_STRING);
  if (length) {
    strncpy(data, value, length);
  }
  // null terminate string
  data[length] = 0;
  return *this;
}

#if !defined(__linux__)
MyMessage& MyMessage::set(const __FlashStringHelper* value)
{
  uint8_t length = value == NULL ? 0
                   : min(strlen_P(reinterpret_cast<const char *>(value)), (size_t)MAX_PAYLOAD);
  miSetLength(length);
  miSetPayloadType(P_STRING);
  if (length) {
    strncpy_P(data, reinterpret_cast<const char *>(value), length);
  }
  // null terminate string
  data[length] = 0;
  return *this;
}
#endif


MyMessage& MyMessage::set(bool value)
{
  miSetLength(1);
  miSetPayloadType(P_BYTE);
  data[0] = value;
  return *this;
}

MyMessage& MyMessage::set(uint8_t value)
{
  miSetLength(1);
  miSetPayloadType(P_BYTE);
  data[0] = value;
  return *this;
}

MyMessage& MyMessage::set(float value, uint8_t decimals)
{
  miSetLength(5); // 32 bit float + persi
  miSetPayloadType(P_FLOAT32);
  fValue=value;
  fPrecision = decimals;
  return *this;
}

MyMessage& MyMessage::set(uint32_t value)
{
  miSetPayloadType(P_ULONG32);
  miSetLength(4);
  ulValue = value;
  return *this;
}

MyMessage& MyMessage::set(int32_t value)
{
  miSetPayloadType(P_LONG32);
  miSetLength(4);
  lValue = value;
  return *this;
}

MyMessage& MyMessage::set(uint16_t value)
{
  miSetPayloadType(P_UINT16);
  miSetLength(2);
  uiValue = value;
  return *this;
}

MyMessage& MyMessage::set(int16_t value)
{
  miSetPayloadType(P_INT16);
  miSetLength(2);
  iValue = value;
  return *this;
}

bool MyMessage::protocolParse(char *inputString)
{
  char *str, *p, *value=NULL;
  uint8_t bvalue[MAX_PAYLOAD];
  uint8_t blen = 0;
  int i = 0;
  uint8_t command = 0;

  // Extract command data coming on serial line
  for (str = strtok_r(inputString, ";", &p); // split using semicolon
          str && i < 6; // loop while str is not null an max 5 times
          str = strtok_r(NULL, ";", &p) // get subsequent tokens
      ) {
    switch (i) {
    case 0: // Radio id (destination)
      this->destination = atoi(str);
      break;
    case 1: // Child id
      this->sensor = atoi(str);
      break;
    case 2: // Message type
      command = atoi(str);
      miSetCommand(command);
      break;
    case 3: // Should we request ack from destination?
      miSetRequestAck(atoi(str)?1:0);
      break;
    case 4: // Data type
      this->type = atoi(str);
      break;
    case 5: // Variable value
      if (command == C_STREAM) {
        while (*str) {
          uint8_t val;
          val = protocolH2i(*str++) << 4;
          val += protocolH2i(*str++);
          bvalue[blen] = val;
          blen++;
        }
      } else {
        value = str;
        // Remove trailing carriage return and newline character (if it exists)
        uint8_t lastCharacter = strlen(value)-1;
        if (value[lastCharacter] == '\r') {
          value[lastCharacter] = 0;
        }
        if (value[lastCharacter] == '\n') {
          value[lastCharacter] = 0;
        }
      }
      break;
    }
    i++;
  }
  // Check for invalid input
  if (i < 5) {
    return false;
  }
  this->sender = GATEWAY_ADDRESS;
  this->last = GATEWAY_ADDRESS;
  miSetAck(false);
  if (command == C_STREAM) {
    this->set(bvalue, blen);
  } else {
    this->set(value);
  }
  return true;
}

char * MyMessage::protocolFormat()
{
  snprintf_P(_fmtBuffer, MY_GATEWAY_MAX_SEND_LENGTH,
             PSTR("%" PRIu8 ";%" PRIu8 ";%" PRIu8 ";%" PRIu8 ";%" PRIu8 ";%s\n"), this->sender,
             this->sensor, (uint8_t)miGetCommand(), (uint8_t)miGetAck(), this->type,
             this->getString(_convBuffer));
  return _fmtBuffer;
}

uint8_t MyMessage::protocolH2i(char c)
{
  uint8_t i = 0;
  if (c <= '9') {
    i += c - '0';
  } else if (c >= 'a') {
    i += c - 'a' + 10;
  } else {
    i += c - 'A' + 10;
  }
  return i;
}
