
#ifndef VITALS_HB_HELPER_H
#define VITALS_HB_HELPER_H

#include <stdint.h>

// vitalsHBStorage: store HB info with mutex protection
void initStorage(void);
void HB_Value_Update(int HBIndex, int16_t HBTime);
void get_and_clear_HB_Values(int32_t* HBMaskRetriever, int16_t* HBTimesRetriever);

// HBDataSend: format and send HB info over Lora
void format_and_send_HB_info(int32_t HBMask, const int16_t* HBTimes);

#endif
