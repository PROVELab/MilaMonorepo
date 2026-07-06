#![allow(dead_code)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(unused_imports)]

//generated Constants
pub const numberOfNodes: u32 = 5;
pub const totalNumFrames: u32 = 9;
pub const numVitalsToTelemPackets: u32 = 7;

//Explicilty defined in sensors.def constants
pub const pointsPerData: u32 = 8;		// 8
pub const nodeIDSizeBits : u32 = 7;		//  7
pub const slowestNodeCount : u32 = 3;		//  3
pub const nodeCount: u32 = 5;		// 5
pub const maxFrameCntBits: u32 = 2;		// 2
pub const maxDataInFrameBits: u32 = 4;		// 4

// global enum specialIDs
pub mod specialIDs {
    pub const telemetryID: u32 = 0;	// 0
    pub const vitalsID: u32 = 2;	// 2
    pub const prechargeID: u32 = 3;	// 3
    pub const powerDistributionID: u32 = 10;	// 10
}
pub use specialIDs::*;

// global enum functionCodes
pub mod functionCodes {
    pub const reservedForMotorController_1: u32 = 0;	// 0b0000
    pub const reservedForMotorController_2: u32 = 1;	// 0b0001
    pub const vitalsCommand: u32 = 2;	// 0b010
    pub const warningCode: u32 = 3;	// 0b0011
    pub const TelemetryCommand: u32 = 4;	// 0b0100
    pub const statusUpdate: u32 = 5;	// 0b0101
    pub const HBPing: u32 = 6;	// 0b0110
    pub const HBPong: u32 = 7;	// 0b0111
    pub const transmitData: u32 = 8;	// 0b1000
}
pub use functionCodes::*;

// global enum vitalsCommands
pub mod vitalsCommands {
    pub const enableContactor: u32 = 0;	// 0b0
    pub const disableContactor: u32 = 1;	// 0b1
}
pub use vitalsCommands::*;

// global enum telem_to_vitals_Commands
pub mod telem_to_vitals_Commands {
    pub const lowPowerLora: u32 = 0;	// 0
    pub const highPowerLora: u32 = 1;	// 1
    pub const enablePrechargeIfSafe: u32 = 2;	// 2
    pub const disablePrecharge: u32 = 3;	// 3
}
pub use telem_to_vitals_Commands::*;

// global enum prechargeState
pub mod prechargeState {
    pub const Off: u32 = 0;	// 0
    pub const Precharging: u32 = 1;	// 1
    pub const On: u32 = 2;	// 2
}
pub use prechargeState::*;

// global enum TWAI_STATE
pub mod TWAI_STATE {
    pub const TWAI_PECAN_STOPPED: u32 = 0;	// 0
    pub const TWAI_PECAN_RUNNING: u32 = 1;	// 1
    pub const TWAI_PECAN_BUS_OFF: u32 = 2;	// 2
    pub const TWAI_PECAN_RECOVERING: u32 = 3;	// 3
}
pub use TWAI_STATE::*;

// global enum vitalsContactorState
pub mod vitalsContactorState {
    pub const allOff: u32 = 0;	// 0
    pub const waitingForIntermoduleContactorEnable: u32 = 1;	// 1
    pub const waitingForPrechargeContactorEnable: u32 = 2;	// 2
    pub const allOn: u32 = 3;	// 3
}
pub use vitalsContactorState::*;

// global enum statusUpdates
pub mod statusUpdates {
    pub const initFlag: u32 = 0;	// 0
    pub const canRecoveryFlag: u32 = 1;	// 1
    pub const canRXOverunFlag: u32 = 2;	// 2
    pub const telemetryCommandAck: u32 = 3;	// 3
    pub const contactorsSuccess: u32 = 4;	// 4
    pub const contactorsFailed: u32 = 5;	// 5
}
pub use statusUpdates::*;

// global enum dataErrorTrigger
pub mod dataErrorTrigger {
    pub const singleCritical: u32 = 0;	// 0
    pub const extrap4: u32 = 1;	// 1
    pub const extrap8: u32 = 2;	// 2
    pub const confirmedCritical: u32 = 3;	// 3
    pub const enteredWarningRange: u32 = 4;	// 4
}
pub use dataErrorTrigger::*;

// global enum frameErrorTrigger
pub mod frameErrorTrigger {
    pub const dataTimeout: u32 = 0;	// 0
    pub const repeatedDataTimeout: u32 = 1;	// 1
    pub const resetTimerError: u32 = 2;	// 2
}
pub use frameErrorTrigger::*;
