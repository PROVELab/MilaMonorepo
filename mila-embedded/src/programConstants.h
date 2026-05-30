#ifndef progConsts
#define progConsts

//generated Constants
#define numberOfNodes 5
#define totalNumFrames 8
#define numMissingIDs 4
#define startingOffset 3
#define numVitalsToTelemPackets 9

//Explicilty defined in sensors.def constants
#define nullID  0		//  0
#define pointsPerData 8		// 8
#define nodeIDSizeBits  7		//  7
#define slowestNodeCount  3		//  3
#define nodeCount 5		// 5
#define maxFrameCntBits 2		// 2
#define maxDataInFrameBits 4		// 4

// global enum specialIDs
typedef enum {
	vitalsID = 2,	/* 2 */
	prechargeID = 3,	/* 3 */
	telemetryID = 4	/* 4 */
} specialIDs;

// global enum functionCodes
typedef enum {
	reservedForMotorController_1 = 0,	/* 0b0000 */
	reservedForMotorController_2 = 1,	/* 0b0001 */
	warningCode = 2,	/* 0b0010 */
	TelemetryCommand = 3,	/* 0b0011 */
	statusUpdate = 4,	/* 0b0100 */
	HBPing = 5,	/* 0b0101 */
	HBPong = 6,	/* 0b0110 */
	transmitData = 7	/* 0b0111 */
} functionCodes;

// global enum vitalsCommands
typedef enum {
	lowPowerLora = 0,	/* 0 */
	highPowerLora = 1	/* 1 */
} vitalsCommands;

// global enum prechargeCommands
typedef enum {
	prechargeRemoveLatch = 0,	/* 0 */
	prechargeLatchOff = 1,	/* 1 */
	prechargeLatchOn = 2	/* 2 */
} prechargeCommands;

// global enum TWAI_STATE
typedef enum {
	TWAI_PECAN_STOPPED = 0,	/* 0 */
	TWAI_PECAN_RUNNING = 1,	/* 1 */
	TWAI_PECAN_BUS_OFF = 2,	/* 2 */
	TWAI_PECAN_RECOVERING = 3	/* 3 */
} TWAI_STATE;

// global enum statusUpdates
typedef enum {
	initFlag = 0,	/* 0 */
	canRecoveryFlag = 1,	/* 1 */
	canRXOverunFlag = 2,	/* 2 */
	telemetryCommandAck = 3	/* 3 */
} statusUpdates;

// global enum prechargeState
typedef enum {
	On = 0,	/* 0 */
	Charging = 1,	/* 1 */
	Off = 2	/* 2 */
} prechargeState;

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
