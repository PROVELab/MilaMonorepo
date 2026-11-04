#pragma once
#include <stddef.h>
#include <stdint.h>
void rfStationStart();
bool rfStationUsbCommand(const uint8_t *data, size_t length);
