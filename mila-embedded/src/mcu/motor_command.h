#pragma once


// Tagged Union description for what control mode
// the motor is in (this is lowk so much easier in rust :sob:)
typedef enum {
    COMMAND_STATE_OFF = 0,

    COMMAND_STATE_PEDAL = 1,
    COMMAND_STATE_CRUISE = 2
} MOTOR_COMMAND_STATE;

// typedef struct {
// } off_state_data;

// These are set once and latch

typedef struct {
    float deadzone; // 0 - 1.0 [0, 100%] how much to ignore
    enum DIRECTION {
        REVERSE = -1,
        FORWARD = 1
    } direction;
} pedal_state_data;

typedef struct {
    float cruise_speed_mph;
} cruise_state_data;

// Tagged Union:
typedef struct {
    MOTOR_COMMAND_STATE tag;

    union {
        // off_state_data off_state;
        pedal_state_data pedal_state;
        cruise_state_data cruise_state;
    } data;
} motor_command;
