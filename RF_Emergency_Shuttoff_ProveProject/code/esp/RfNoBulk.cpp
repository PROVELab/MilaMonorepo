#include "LoraCommon/LoraProtocol.h"
#include "LoraCommon/LoraErrLog.hpp"
#include "esp_log.h"
/* RF-only review mode deliberately rejects bulk traffic. Do not silently queue
 * telemetry that cannot fit. Restore streaming only after a bounded slot design
 * has been reviewed and tested, or provide a separate radio. */
extern "C" bool protocolTransmit(uint8_t *, uint8_t) { return false; }
extern "C" bool protocolRecv(driverRecvPacket *) { return false; }
extern "C" void getBitmap(uint16_t *bitmap, uint8_t *size) { *bitmap=0; *size=0; }
void initErr() {}
void logErr(const char *tag, int16_t error) { ESP_LOGE(tag,"RF review error %d",error); }
uint8_t getErrorPacket(int16_t *) { return 0; } // No bulk error transport in this profile.
