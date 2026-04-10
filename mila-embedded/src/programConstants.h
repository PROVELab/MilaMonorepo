#ifndef progConsts
#define progConsts

//generated Constants
#define numberOfNodes 1
#define totalNumFrames 1
#define numMissingIDs 0
#define startingOffset 10

//Explicilty defined in sensors.def constants
#define nullID  0		//  0
#define pointsPerData 10		// 10
#define nodeIDSizeBits  7		//  7
#define nonCriticalWarning 2		//  0b010
#define CriticalWarning  4		//  0b100
#define warningTypeMask  7		//  0b111
#define warningNodeFlagIndex  11		//  11
#define warningFrameFlagIndex 18		// 18
#define maxFrameCntBits 3		// 3
#define warningDataFlagIndex 21		// 21
#define maxDataInFrameBits 3		// 3
#define HBupdateTypeBits  1		//  1
#define HBupdateStatus  0		//  0b0
#define HBupdateTiming  1		//  0b1
#define slowestNodeCount  3		//  3
#define HBStatusFrameBits  1		//  1
#define HBTimerMSBits  10		//  10
#define frame0FillerBits  11		//  11

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

// global enum telemetryCommandFlags
typedef enum {
	enablePrecharge = 4,	/* 4 */
	disablePrecharge = 5,	/* 5 */
	telemetryCommandCRCError = 7,	/* 7 */
	customChangeDataFlag = 9	/* 9 */
} telemetryCommandFlags;

// global enum TWAI_STATE
typedef enum {
	TWAI_STATE_STOPPED = 0,	/* 0 */
	TWAI_STATE_RUNNING = 1,	/* 1 */
	TWAI_STATE_BUS_OFF = 2,	/* 2 */
	TWAI_STATE_RECOVERING = 3	/* 3 */
} TWAI_STATE;

// global enum statusUpdates
typedef enum {
	initFlag = 0,	/* 0b00000000 */
	canRecoveryFlag = 1,	/* 0b00000001 */
	canRXOverunFlag = 2	/* 0b00000010 */
} statusUpdates;

// global enum prechargeStatusUpdates
typedef enum {
	On_Charging = 5,	/* 0b00000101 */
	On_FinishedCharging = 6,	/* 0b00000110 */
	Off = 7	/* 0b00000111 */
} prechargeStatusUpdates;

// global enum TWAI_State
typedef enum {
	TWAI_Stopped = 0,	/* 0 */
	TWAI_Running = 1,	/* 1 */
	TWAI_BusOff = 2,	/* 2 */
	TWAI_Recovering = 3	/* 3 */
} TWAI_State;

// global enum extrapolationTrigger
typedef enum {
	extrap10 = 0,	/* 0 */
	extrap5 = 1,	/* 1 */
	doubleCritical = 2	/* 2 */
} extrapolationTrigger;

#endif
