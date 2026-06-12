#ifndef VITALS_SEND_DATA_H
#define VITALS_SEND_DATA_H

#include <stdint.h>
#include <stddef.h>
#include "../pecan/pecan.h"

uint8_t formatPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* tempData);
uint8_t formatPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes, uint8_t* outBuffer);

void sendPacketVariable(const simpleDataPoint* fields, size_t numFields, const int32_t* data, const uint8_t* payload, size_t payloadBytes);
void sendPacketCore(const simpleDataPoint* fields, size_t numFields, const int32_t* data, uint8_t* dataBuffer);

#endif