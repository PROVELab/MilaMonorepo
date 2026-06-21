#ifndef COOLANT_H
#define COOLANT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_coolant(void);
void setCoolantDutyCycle(int32_t duty_0_to_100);

// --- Thread-Safe Getters ---
int32_t get_coolant_duty_cycle(void);
int32_t get_coolant_fault_active(void);
int32_t get_coolant_freq_khz(void);
int32_t get_coolant_avg_current_mA(void);
int32_t get_coolant_peak_current_mA(void);

#ifdef __cplusplus
}
#endif

#endif // COOLANT_H