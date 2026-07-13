package util;
public final class Constants {
    private Constants() {}

    //generated Constants
    public static final int numberOfNodes = 5;
    public static final int totalNumFrames = 9;
    public static final int numVitalsToTelemPackets = 7;

    //Explicilty defined in sensors.def constants
    public static final int pointsPerData = 8;		// 8
    public static final int nodeIDSizeBits  = 7;		//  7
    public static final int slowestNodeCount  = 3;		//  3
    public static final int nodeCount = 5;		// 5
    public static final int maxFrameCntBits = 2;		// 2
    public static final int maxDataInFrameBits = 4;		// 4

	// global enum specialIDs
	public static final class specialIDs {
		private specialIDs() {}
		public static final int telemetryID = 0;	// 0
		public static final int vitalsID = 2;	// 2
		public static final int prechargeID = 3;	// 3
		public static final int powerDistributionID = 10;	// 10
	}

	// global enum functionCodes
	public static final class functionCodes {
		private functionCodes() {}
		public static final int reservedForMotorController_1 = 0;	// 0b0000
		public static final int reservedForMotorController_2 = 1;	// 0b0001
		public static final int vitalsCommand = 2;	// 0b010
		public static final int warningCode = 3;	// 0b0011
		public static final int TelemetryCommand = 4;	// 0b0100
		public static final int statusUpdate = 5;	// 0b0101
		public static final int HBPing = 6;	// 0b0110
		public static final int HBPong = 7;	// 0b0111
		public static final int transmitData = 8;	// 0b1000
	}

	// global enum vitalsCommands
	public static final class vitalsCommands {
		private vitalsCommands() {}
		public static final int enableContactor = 0;	// 0b0
		public static final int disableContactor = 1;	// 0b1
	}

	// global enum telem_to_vitals_Commands
	public static final class telem_to_vitals_Commands {
		private telem_to_vitals_Commands() {}
		public static final int lowPowerLora = 0;	// 0
		public static final int highPowerLora = 1;	// 1
		public static final int enablePrechargeIfSafe = 2;	// 2
		public static final int disablePrecharge = 3;	// 3
	}

	// global enum prechargeState
	public static final class prechargeState {
		private prechargeState() {}
		public static final int Off = 0;	// 0
		public static final int Precharging = 1;	// 1
		public static final int On = 2;	// 2
	}

	// global enum TWAI_STATE
	public static final class TWAI_STATE {
		private TWAI_STATE() {}
		public static final int TWAI_PECAN_STOPPED = 0;	// 0
		public static final int TWAI_PECAN_RUNNING = 1;	// 1
		public static final int TWAI_PECAN_BUS_OFF = 2;	// 2
		public static final int TWAI_PECAN_RECOVERING = 3;	// 3
	}

	// global enum vitalsContactorState
	public static final class vitalsContactorState {
		private vitalsContactorState() {}
		public static final int allOff = 0;	// 0
		public static final int waitingForIntermoduleContactorEnable = 1;	// 1
		public static final int waitingForPrechargeContactorEnable = 2;	// 2
		public static final int allOn = 3;	// 3
	}

	// global enum statusUpdates
	public static final class statusUpdates {
		private statusUpdates() {}
		public static final int initFlag = 0;	// 0
		public static final int canRecoveryFlag = 1;	// 1
		public static final int canRXOverunFlag = 2;	// 2
		public static final int telemetryCommandAck = 3;	// 3
		public static final int contactorsSuccess = 4;	// 4
		public static final int contactorsFailed = 5;	// 5
	}

	// global enum dataErrorTrigger
	public static final class dataErrorTrigger {
		private dataErrorTrigger() {}
		public static final int singleCritical = 0;	// 0
		public static final int extrap4 = 1;	// 1
		public static final int extrap8 = 2;	// 2
		public static final int confirmedCritical = 3;	// 3
		public static final int enteredWarningRange = 4;	// 4
	}

	// global enum frameErrorTrigger
	public static final class frameErrorTrigger {
		private frameErrorTrigger() {}
		public static final int dataTimeout = 0;	// 0
		public static final int repeatedDataTimeout = 1;	// 1
		public static final int resetTimerError = 2;	// 2
	}

	public static final int[] nodeIDs = new int[]{ 3, 7, 8, 9, 10 };
}
