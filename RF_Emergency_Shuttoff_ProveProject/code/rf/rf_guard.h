#ifndef RF_GUARD_H
#define RF_GUARD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* All times are monotonic microseconds, not wall-clock time. */
#define RF_REQUEST_US 60000u
#define RF_TIMEOUT_US 200000u
#define RF_REPLY_WINDOW_US 50000u
#define RF_FRAME_SIZE 24u
#define RF_GOOD_REPLIES_TO_RESET 3u

enum RfKind { RF_REQUEST = 1, RF_RESPONSE = 2 };
enum RfReason {
    RF_NONE, RF_STARTUP, RF_TIMEOUT, RF_STATION_STOP,
    RF_NODE_LOST, RF_RADIO_ERROR, RF_SEQUENCE_END
};
typedef struct {
    uint8_t kind;
    uint64_t session;
    uint32_t sequence;
    uint8_t permit; /* Request: car permit. Response: station permit. */
    uint8_t reason;
} RfFrame;

typedef struct {
    uint64_t session, started_us, last_good_us, sent_us;
    uint32_t sequence;
    uint8_t reason, good_count;
    bool fault, have_good, pending, nodes_ready;
} RfGuard;

/* These functions have no I/O and never wait. The caller owns synchronization. */
void rf_init(RfGuard *g, uint64_t session, uint64_t now);
void rf_trip(RfGuard *g, uint8_t reason);
void rf_tick(RfGuard *g, uint64_t now);
void rf_nodes_ready(RfGuard *g, bool ready);
bool rf_make_request(RfGuard *g, uint64_t now, RfFrame *out);
bool rf_receive(RfGuard *g, const RfFrame *reply, uint64_t now);
bool rf_reset(RfGuard *g, uint64_t now, bool deliberate_local_reset,
              bool physically_off_confirmed);
bool rf_permit(const RfGuard *g);
void rf_encode(const RfFrame *f, uint8_t out[RF_FRAME_SIZE]);
bool rf_decode(const uint8_t *data, size_t length, RfFrame *f);
#ifdef __cplusplus
}
#endif
#endif
