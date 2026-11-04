#include "rf_station.h"
#include <string.h>
void rf_station_init(RfStation *s) { memset(s, 0, sizeof(*s)); }
void rf_station_command(RfStation *s, uint8_t command, uint64_t now) {
    if (s->allowed && now - s->lease_us > RF_OPERATOR_LEASE_US) s->allowed = false;
    if (command == 0) s->allowed = false; /* STOP always wins. */
    if (command == 1 && s->session) s->allowed = true; /* Explicit allow. */
    if (s->allowed && (command == 1 || command == 2)) s->lease_us = now;
}
bool rf_station_reply(RfStation *s, const RfFrame *q, uint64_t now, RfFrame *r) {
    if (q->kind != RF_REQUEST || !q->session || !q->sequence) return false;
    if (q->session != s->session) {
        /* A restarted car needs another deliberate station allow action. */
        s->session = q->session; s->sequence = 0; s->allowed = false;
    }
    if (q->sequence <= s->sequence) return false;
    s->sequence = q->sequence;
    if (now - s->lease_us > RF_OPERATOR_LEASE_US) s->allowed = false;
    *r = *q;
    r->kind = RF_RESPONSE;
    r->permit = s->allowed ? 1 : 0;
    r->reason = s->allowed ? RF_NONE : RF_STATION_STOP;
    return true; /* Build now; never replay a queued telemetry ACK. */
}
