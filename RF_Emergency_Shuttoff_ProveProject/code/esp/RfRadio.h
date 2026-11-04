#pragma once
#include "../rf/rf_guard.h"
bool rfRadioInit();
bool rfRadioSend(const RfFrame &frame, uint64_t deadline);
bool rfRadioRead(RfFrame &frame, uint64_t deadline);
