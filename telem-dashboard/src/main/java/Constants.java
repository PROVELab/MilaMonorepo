public final class Constants {
    private Constants() {}

    //generated Constants
    public static final int numberOfNodes = 5;
    public static final int totalNumFrames = 8;
    public static final int numMissingIDs = 4;
    public static final int startingOffset = 3;

    //Explicilty defined in sensors.def constants
    public static final int nullID  = 0;		//  0
    public static final int pointsPerData = 10;		// 10
    public static final int nodeIDSizeBits  = 7;		//  7
    public static final int slowestNodeCount  = 3;		//  3
    public static final int nodeCount = 5;		// 5
    public static final int maxFrameCntBits = 2;		// 2
    public static final int maxDataInFrameBits = 4;		// 4

	// global enum specialIDs
	public static final class specialIDs {
		private specialIDs() {}
		public static final int vitalsID = 2;	// 2
		public static final int prechargeID = 3;	// 3
		public static final int telemetryID = 4;	// 4
	}

	// global enum functionCodes
	public static final class functionCodes {
		private functionCodes() {}
		public static final int reservedForMotorController_1 = 0;	// 0b0000
		public static final int reservedForMotorController_2 = 1;	// 0b0001
		public static final int warningCode = 2;	// 0b0010
		public static final int TelemetryCommand = 3;	// 0b0011
		public static final int statusUpdate = 4;	// 0b0100
		public static final int HBPing = 5;	// 0b0101
		public static final int HBPong = 6;	// 0b0110
		public static final int transmitData = 7;	// 0b0111
	}

	// global enum vitalsCommands
	public static final class vitalsCommands {
		private vitalsCommands() {}
		public static final int lowPowerLora = 0;	// 0
	}

	// global enum prechargeCommands
	public static final class prechargeCommands {
		private prechargeCommands() {}
		public static final int prechargeRemoveLatch = 0;	// 0
		public static final int prechargeLatchOff = 1;	// 1
		public static final int prechargeLatchOn = 2;	// 2
	}

	// global enum TWAI_STATE
	public static final class TWAI_STATE {
		private TWAI_STATE() {}
		public static final int TWAI_PECAN_STOPPED = 0;	// 0
		public static final int TWAI_PECAN_RUNNING = 1;	// 1
		public static final int TWAI_PECAN_BUS_OFF = 2;	// 2
		public static final int TWAI_PECAN_RECOVERING = 3;	// 3
	}

	// global enum statusUpdates
	public static final class statusUpdates {
		private statusUpdates() {}
		public static final int initFlag = 0;	// 0
		public static final int canRecoveryFlag = 1;	// 1
		public static final int canRXOverunFlag = 2;	// 2
		public static final int telemetryCommandAck = 3;	// 3
	}

	// global enum prechargeState
	public static final class prechargeState {
		private prechargeState() {}
		public static final int On = 0;	// 0
		public static final int Charging = 1;	// 1
		public static final int Off = 2;	// 2
	}

	// global enum errorTrigger
	public static final class errorTrigger {
		private errorTrigger() {}
		public static final int warning_nonCritical = 0;	// 0
		public static final int singleCritical = 1;	// 1
		public static final int extrap5 = 2;	// 2
		public static final int extrap10 = 3;	// 3
		public static final int confirmedCritical = 4;	// 4
	}

	public static final int[] nodeIDs = new int[]{ 3, 8, 9, 10, 11 };
}
