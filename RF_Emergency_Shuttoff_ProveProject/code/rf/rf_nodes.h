#ifndef RF_NODES_H
#define RF_NODES_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RF_NODE_COUNT 4
#define RF_CAN_PERIOD_US 50000u
#define RF_CAN_REPLY_US 40000u
#define RF_CAN_LOSS_US 150000u
/* CAN data: version(1), flags(1), boot epoch(4), counter(2), little endian.
 * Status flags: bit 0 = RF permit. Reply flags: bit 0 = node ready.
 * A reply means the node ran its safety check, not merely received CAN. */
typedef struct {
    uint32_t epoch;
    uint16_t counter;
    uint64_t sent_us, last_reply[RF_NODE_COUNT];
    bool seen[RF_NODE_COUNT], replied[RF_NODE_COUNT];
} RfNodes;
typedef struct {
    uint32_t epoch;
    uint16_t counter;
    uint64_t last_us;
    bool seen, latched, remote_permit;
} RfNodeGuard;
void rf_nodes_init(RfNodes *n, uint32_t epoch);
bool rf_nodes_poll(RfNodes *n, uint64_t now, bool permit, uint8_t data[8]);
bool rf_nodes_reply(RfNodes *n, unsigned index, const uint8_t data[8], uint64_t now);
bool rf_nodes_all_ready(const RfNodes *n, uint64_t now);
void rf_node_init(RfNodeGuard *n);
bool rf_node_receive(RfNodeGuard *n, const uint8_t data[8], uint64_t now,
                     bool local_ready, uint8_t reply[8]);
bool rf_node_must_stop(RfNodeGuard *n, uint64_t now);
bool rf_node_reset(RfNodeGuard *n, uint64_t now, bool deliberate, bool off);
#ifdef __cplusplus
}
#endif
#endif
