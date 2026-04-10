#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <cstddef>
#include "Driver/Config.hpp"   //RadioConfig
#include "blastProtocolConfig.hpp"   //protocol packet formats and config

//set up build_src_filter so these functions link to the correct things

//common between protocols
//returns error code on crash. Can call again to re-start protocol if it returns
int16_t runProtocol(const RadioConfig* config, char*& errorMsg);
//

//custom per protocol
bool protocolTransmit(uint8_t* data, uint8_t dataLen);
bool protocolRecv(driverRecvPacket* msg);
//

//only usable for RX side. get the latest bitmap to see what percentage are coming through on each burst
void getBitmap(uint16_t& bitmap, uint8_t& burstSize);

//for internal use (in protocol common):
bool validatePacketHeader(driverRecvPacket* driverPacket, size_t expectedHeaderLength);
void printRecvStatus(driverRecvPacket* info);

#endif