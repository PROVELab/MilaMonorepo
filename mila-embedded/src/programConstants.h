#ifndef progConsts
#define progConsts

//generated Constants
#define numberOfNodes 5
#define totalNumFrames 9
#define numVitalsToTelemPackets 7

//Explicilty defined in sensors.def constants
#define pointsPerData 8		// 8
#define nodeIDSizeBits  7		//  7
#define slowestNodeCount  3		//  3
#define nodeCount 5		// 5
#define maxFrameCntBits 2		// 2
#define maxDataInFrameBits 4		// 4

// global enum specialIDs
typedef enum {
	telemetryID = 0,	/* 0 */
	vitalsID = 2,	/* 2 */
	prechargeID = 3,	/* 3 */
	powerDistributionID = 10	/* 10 */
} specialIDs;

// global enum functionCodes
typedef enum {
	reservedForMotorController_1 = 0,	/* 0b0000 */
	reservedForMotorController_2 = 1,	/* 0b0001 */
	vitalsCommand = 2,	/* 0b010 */
	warningCode = 3,	/* 0b0011 */
	TelemetryCommand = 4,	/* 0b0100 */
	statusUpdate = 5,	/* 0b0101 */
	HBPing = 6,	/* 0b0110 */
	HBPong = 7,	/* 0b0111 */
	transmitData = 8	/* 0b1000 */
} functionCodes;

// global enum vitalsCommands
typedef enum {
	enableContactor = 0,	/* 0b0 */
	disableContactor = 1	/* 0b1 */
} vitalsCommands;

// global enum telem_to_vitals_Commands
typedef enum {
	lowPowerLora = 0,	/* 0 */
	highPowerLora = 1,	/* 1 */
	enablePrechargeIfSafe = 2,	/* 2 */
	disablePrecharge = 3	/* 3 */
} telem_to_vitals_Commands;

// global enum prechargeState
typedef enum {
	Off = 0,	/* 0 */
	Precharging = 1,	/* 1 */
	On = 2	/* 2 */
} prechargeState;

// global enum TWAI_STATE
typedef enum {
	TWAI_PECAN_STOPPED = 0,	/* 0 */
	TWAI_PECAN_RUNNING = 1,	/* 1 */
	TWAI_PECAN_BUS_OFF = 2,	/* 2 */
	TWAI_PECAN_RECOVERING = 3	/* 3 */
} TWAI_STATE;

// global enum vitalsContactorState
typedef enum {
	allOff = 0,	/* 0 */
	waitingForIntermoduleContactorEnable = 1,	/* 1 */
	waitingForPrechargeContactorEnable = 2,	/* 2 */
	allOn = 3	/* 3 */
} vitalsContactorState;

// global enum statusUpdates
typedef enum {
	initFlag = 0,	/* 0 */
	canRecoveryFlag = 1,	/* 1 */
	canRXOverunFlag = 2,	/* 2 */
	telemetryCommandAck = 3,	/* 3 */
	contactorsSuccess = 4,	/* 4 */
	contactorsFailed = 5	/* 5 */
} statusUpdates;

// global enum dataErrorTrigger
typedef enum {
	singleCritical = 0,	/* 0 */
	extrap4 = 1,	/* 1 */
	extrap8 = 2,	/* 2 */
	confirmedCritical = 3,	/* 3 */
	enteredWarningRange = 4	/* 4 */
} dataErrorTrigger;

// global enum frameErrorTrigger
typedef enum {
	dataTimeout = 0,	/* 0 */
	repeatedDataTimeout = 1,	/* 1 */
	resetTimerError = 2	/* 2 */
} frameErrorTrigger;

#endif
