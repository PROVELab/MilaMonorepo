#include "rf_guard.h"
#include <string.h>

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffff;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (unsigned i = 0; i < 8; ++i)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
}

void rf_init(RfGuard *g, uint64_t session, uint64_t now) {
    memset(g, 0, sizeof(*g));
    g->session = session;
    g->started_us = now;
    g->fault = true; /* Restarting never gives permission to energize. */
    g->reason = RF_STARTUP;
}

void rf_trip(RfGuard *g, uint8_t reason) {
    /* Keep the first operating fault so later failures do not hide it. */
    if (!g->fault || g->reason == RF_STARTUP) g->reason = reason;
    g->fault = true;
    g->good_count = 0;
}

void rf_tick(RfGuard *g, uint64_t now) {
    uint64_t reference = g->have_good ? g->last_good_us : g->started_us;
    if (now - reference > RF_TIMEOUT_US) rf_trip(g, RF_TIMEOUT);
    if (g->pending && now - g->sent_us > RF_REPLY_WINDOW_US) {
        g->pending = false;
        g->good_count = 0;
    }
}

void rf_nodes_ready(RfGuard *g, bool ready) {
    g->nodes_ready = ready;
    if (!ready) {
        g->good_count = 0;
        if (!g->fault) rf_trip(g, RF_NODE_LOST);
    }
}

bool rf_make_request(RfGuard *g, uint64_t now, RfFrame *out) {
    rf_tick(g, now);
    if (g->sequence && now - g->sent_us < RF_REQUEST_US) return false;
    if (g->sequence == UINT32_MAX || g->session == 0) {
        rf_trip(g, RF_SEQUENCE_END);
        return false; /* New session and explicit reset are required. */
    }
    g->sequence++;
    g->sent_us = now;
    g->pending = true;
    *out = (RfFrame){RF_REQUEST, g->session, g->sequence,
                     rf_permit(g) ? 1 : 0, g->reason};
    return true;
}

bool rf_receive(RfGuard *g, const RfFrame *f, uint64_t now) {
    /* Check expiry FIRST. A late reply cannot erase a missed deadline. */
    rf_tick(g, now);
    if (f->kind != RF_RESPONSE || f->session != g->session ||
        !g->pending || f->sequence != g->sequence ||
        now - g->sent_us > RF_REPLY_WINDOW_US || f->permit > 1) return false;
    g->pending = false; /* The same reply cannot refresh the clock twice. */
    if (!f->permit) {
        rf_trip(g, RF_STATION_STOP);
        return true;
    }
    g->last_good_us = now;
    g->have_good = true;
    if (g->good_count < RF_GOOD_REPLIES_TO_RESET) g->good_count++;
    return true; /* This deliberately does NOT clear the fault. */
}

bool rf_reset(RfGuard *g, uint64_t now, bool local, bool off) {
    rf_tick(g, now);
    if (!local || !off || !g->nodes_ready || !g->have_good ||
        now - g->last_good_us > RF_REPLY_WINDOW_US ||
        g->good_count < RF_GOOD_REPLIES_TO_RESET || !g->session ||
        g->sequence == UINT32_MAX) return false;
    g->fault = false;
    g->reason = RF_NONE;
    return true; /* Permission only; no contactor-enable command is sent. */
}

bool rf_permit(const RfGuard *g) { return !g->fault && g->nodes_ready; }

void rf_encode(const RfFrame *f, uint8_t out[RF_FRAME_SIZE]) {
    memset(out, 0, RF_FRAME_SIZE);
    out[0] = 'R'; out[1] = 'H'; out[2] = 1; out[3] = f->kind;
    for (unsigned i = 0; i < 8; ++i) out[4+i] = (uint8_t)(f->session >> (8*i));
    for (unsigned i = 0; i < 4; ++i) out[12+i] = (uint8_t)(f->sequence >> (8*i));
    out[16] = f->permit; out[17] = f->reason;
    uint16_t crc = crc16(out, 22);
    out[22] = (uint8_t)crc; out[23] = (uint8_t)(crc >> 8);
}

bool rf_decode(const uint8_t *p, size_t n, RfFrame *f) {
    if (n != RF_FRAME_SIZE || p[0] != 'R' || p[1] != 'H' || p[2] != 1 ||
        (p[3] != RF_REQUEST && p[3] != RF_RESPONSE) || p[16] > 1 ||
        p[17] > RF_SEQUENCE_END || p[18] || p[19] || p[20] || p[21] ||
        crc16(p, 22) != ((uint16_t)p[22] | (uint16_t)p[23] << 8)) return false;
    memset(f, 0, sizeof(*f));
    f->kind = p[3]; f->permit = p[16]; f->reason = p[17];
    for (unsigned i = 0; i < 8; ++i) f->session |= (uint64_t)p[4+i] << (8*i);
    for (unsigned i = 0; i < 4; ++i) f->sequence |= (uint32_t)p[12+i] << (8*i);
    return f->session != 0 && f->sequence != 0;
}
