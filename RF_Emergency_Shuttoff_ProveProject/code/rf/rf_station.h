#ifndef RF_STATION_H
#define RF_STATION_H
#include "rf_guard.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RF_OPERATOR_LEASE_US 120000u /* Proposed laptop-loss policy. */
typedef struct {
    uint64_t session, lease_us;
    uint32_t sequence;
    bool allowed;
} RfStation;
void rf_station_init(RfStation *s);
void rf_station_command(RfStation *s, uint8_t command, uint64_t now);
bool rf_station_reply(RfStation *s, const RfFrame *request,
                      uint64_t now, RfFrame *reply);
#ifdef __cplusplus
}
#endif
#endif
