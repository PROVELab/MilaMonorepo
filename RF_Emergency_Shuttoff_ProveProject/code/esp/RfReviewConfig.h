#pragma once
/* Leave unassigned until the lead approves the CAN allocation and board map.
 * -1 means NOT configured. The car stays inhibited with these defaults. */
#define RF_STATUS_FUNCTION (-1)
#define RF_REPLY_FUNCTION (-1)
#define RF_LOGIC_NODE (-1)
#define RF_12V_NODE (-1)
#define RF_COMPUTE_NODE (-1)
#define RF_BACKUP_POWER_NODE (-1)
/* This is not a brake pin. Brake hardware and backup power policy are pending. */
#define RF_PHYSICAL_SHUTDOWN_REVIEWED 0

/* Proposed radio profile: short, exclusive RF heartbeat packets.
 * Both ends must agree. SF7/4:5 changes the range/error-rate tradeoff.
 * Keep lowPower for the review profile; do not assume full-power bench use. */
#define RF_REVIEW_SF 7
#define RF_REVIEW_CR 5
