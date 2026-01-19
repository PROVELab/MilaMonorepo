#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../LoraCommon/LoraDriverConfig.hpp"   //RadioConfig

void Lora_RX_Init(const RadioConfig config);
void Lora_RX_Restart(const RadioConfig config);

//returns number of bytes in rxBuffer, and points rxBuffer to the oldest unread Lora msg. (not thread safe)
size_t LoraAPIRead(uint8_t*& rxBuffer);