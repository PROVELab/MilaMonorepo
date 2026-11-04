#ifndef RF_ENDPOINT_H
#define RF_ENDPOINT_H
#include "rf_nodes.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Callbacks must return promptly. The board owner supplies actual hardware I/O.
 * The backup power board must NOT use this primary-node stop adapter. */
typedef struct {
    RfNodeGuard guard;
    void (*request_stop)(void);
    bool (*safety_path_ready)(void);
    bool (*physical_outputs_off)(void);
} RfEndpoint;
bool rf_endpoint_init(RfEndpoint *e, void (*stop)(void), bool (*ready)(void), bool (*off)(void));
void rf_endpoint_tick(RfEndpoint *e, uint64_t now);
bool rf_endpoint_receive(RfEndpoint *e, const uint8_t status[8], uint64_t now, uint8_t reply[8]);
bool rf_endpoint_reset(RfEndpoint *e, uint64_t now, bool deliberate);
#ifdef __cplusplus
}
#endif
#endif
