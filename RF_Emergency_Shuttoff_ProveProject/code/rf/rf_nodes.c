#include "rf_nodes.h"
#include <string.h>
static uint32_t epoch_of(const uint8_t *p) {
    return (uint32_t)p[2] | (uint32_t)p[3]<<8 | (uint32_t)p[4]<<16 | (uint32_t)p[5]<<24;
}
static uint16_t count_of(const uint8_t *p) { return (uint16_t)p[6] | (uint16_t)p[7]<<8; }
static void pack(uint8_t *p, uint32_t epoch, uint16_t count, bool flag) {
    p[0] = 1; p[1] = flag ? 1 : 0;
    for (unsigned i=0; i<4; ++i) p[2+i] = (uint8_t)(epoch>>(8*i));
    p[6] = (uint8_t)count; p[7] = (uint8_t)(count>>8);
}
void rf_nodes_init(RfNodes *n, uint32_t epoch) {
    memset(n, 0, sizeof(*n)); n->epoch = epoch;
}
bool rf_nodes_poll(RfNodes *n, uint64_t now, bool permit, uint8_t p[8]) {
    if (n->counter && now - n->sent_us < RF_CAN_PERIOD_US) return false;
    if (!n->epoch) return false;
    /* Rotate epoch before the 16-bit counter wraps. This is not an RF reset. */
    if (n->counter == UINT16_MAX) {
        if (n->epoch == UINT32_MAX) return false;
        n->epoch++; n->counter = 0;
    }
    n->counter++; n->sent_us = now;
    memset(n->replied, 0, sizeof(n->replied));
    pack(p, n->epoch, n->counter, permit);
    return true;
}
bool rf_nodes_reply(RfNodes *n, unsigned i, const uint8_t p[8], uint64_t now) {
    if (i >= RF_NODE_COUNT || !n->counter || p[0] != 1 || p[1] > 1 ||
        epoch_of(p) != n->epoch || count_of(p) != n->counter ||
        n->replied[i] || now - n->sent_us > RF_CAN_REPLY_US) return false;
    n->replied[i] = true;
    n->seen[i] = p[1] == 1;
    if (n->seen[i]) n->last_reply[i] = now;
    return true;
}
bool rf_nodes_all_ready(const RfNodes *n, uint64_t now) {
    for (unsigned i=0; i<RF_NODE_COUNT; ++i)
        if (!n->seen[i] || now - n->last_reply[i] > RF_CAN_LOSS_US) return false;
    return true;
}
void rf_node_init(RfNodeGuard *n) { memset(n,0,sizeof(*n)); n->latched = true; }
bool rf_node_receive(RfNodeGuard *n, const uint8_t p[8], uint64_t now,
                     bool ready, uint8_t reply[8]) {
    rf_node_must_stop(n, now); /* Latch expiry even if this packet arrived late. */
    if (p[0] != 1 || p[1] > 1 || !epoch_of(p) || !count_of(p)) return false;
    uint32_t epoch = epoch_of(p); uint16_t count = count_of(p);
    if (n->seen && epoch == n->epoch && count <= n->counter) return false;
    if (n->seen && epoch != n->epoch) n->latched = true;
    n->epoch = epoch; n->counter = count; n->seen = true; n->last_us = now;
    n->remote_permit = p[1] == 1;
    if (!n->remote_permit || !ready) n->latched = true;
    pack(reply, epoch, count, ready);
    return true;
}
bool rf_node_must_stop(RfNodeGuard *n, uint64_t now) {
    if (!n->seen || now - n->last_us > RF_CAN_LOSS_US) n->latched = true;
    return n->latched;
}
bool rf_node_reset(RfNodeGuard *n, uint64_t now, bool deliberate, bool off) {
    rf_node_must_stop(n, now);
    if (!deliberate || !off || !n->seen || !n->remote_permit ||
        now - n->last_us > RF_CAN_REPLY_US) return false;
    n->latched = false;
    return true;
}
