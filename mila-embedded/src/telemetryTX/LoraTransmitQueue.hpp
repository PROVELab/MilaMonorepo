#pragma once

#include <stdint.h>

#include "../LoraCommon/blastProtocolConfig.hpp"

void initQueue();

//add a frame to the queue
void addFrameToQueue(uint8_t* frame, uint8_t frameSize);

//take Lora packets out of the queue, and put them in a burstBuffer, for sending a chunk of packets
//returns number of packets in this burst
void refreshBurstBuffer(uint8_t burstBuffer[maxPacketGroupSize][maxLoraPacketSize],
                 uint8_t burstBufferLens[maxPacketGroupSize], uint8_t& burstBufferCount);

