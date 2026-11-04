#include "rf_endpoint.h"
bool rf_endpoint_init(RfEndpoint *e, void (*stop)(void), bool (*ready)(void), bool (*off)(void)) {
    rf_node_init(&e->guard);
    e->request_stop=stop; e->safety_path_ready=ready; e->physical_outputs_off=off;
    if (!stop || !ready || !off) return false;
    stop(); /* Start inhibited, before accepting any CAN permit. */
    return true;
}
void rf_endpoint_tick(RfEndpoint *e, uint64_t now) {
    if (e->safety_path_ready && !e->safety_path_ready()) e->guard.latched=true;
    if (rf_node_must_stop(&e->guard,now) && e->request_stop) e->request_stop();
}
bool rf_endpoint_receive(RfEndpoint *e, const uint8_t status[8], uint64_t now, uint8_t reply[8]) {
    bool ready=e->request_stop && e->safety_path_ready && e->physical_outputs_off && e->safety_path_ready();
    bool accepted=rf_node_receive(&e->guard,status,now,ready,reply);
    rf_endpoint_tick(e,now);
    return accepted;
}
bool rf_endpoint_reset(RfEndpoint *e, uint64_t now, bool deliberate) {
    if (!e->request_stop || !e->safety_path_ready || !e->physical_outputs_off ||
        !e->safety_path_ready()) return false;
    return rf_node_reset(&e->guard,now,deliberate,e->physical_outputs_off());
}
