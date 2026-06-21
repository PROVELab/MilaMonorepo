
#ifndef VITALS_LORA_RECV_HPP
#define VITALS_LORA_RECV_HPP

#include "../../pecan/pecan.h"

#ifdef __cplusplus
extern "C" {
#endif


//the main task
void vitalsLoraInit(const UBaseType_t lora_monitor_priority, const UBaseType_t lora_recv_priority);

//for CAN messages that are immediately forwarded to Lora (these are pecanListParam callbacks)
int16_t forwardStatusUpdate(CANPacket* message);
int16_t vitals_defaultPacketRecv(CANPacket* p); //forward unknown can packets asw

#ifdef __cplusplus
}
#endif

#endif // VITALS_LORA_HPP
