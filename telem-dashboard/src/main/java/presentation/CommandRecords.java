/** Auto-generated file. Do not edit. */

package presentation;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class CommandRecords {
    private CommandRecords() {}

    public record EnumEntry(String name, int value) {}
    public record CommandField(String name, int bits, int min, int max, String enumName) {}
    public record Command(String name, int mask, int maskBits, boolean isCustom, List<CommandField> fields) {
        @Override public String toString() { return name; }
    }

    public static final Map<String, List<EnumEntry>> ENUMS = new HashMap<>();
    public static final List<Command> COMMANDS = new ArrayList<>();

    static {
        List<EnumEntry> specialIDs_entries = new ArrayList<>();
        specialIDs_entries.add(new EnumEntry("telemetryID", 0));
        specialIDs_entries.add(new EnumEntry("vitalsID", 2));
        specialIDs_entries.add(new EnumEntry("prechargeID", 3));
        specialIDs_entries.add(new EnumEntry("powerDistributionID", 10));
        ENUMS.put("specialIDs", specialIDs_entries);

        List<EnumEntry> functionCodes_entries = new ArrayList<>();
        functionCodes_entries.add(new EnumEntry("reservedForMotorController_1", 0));
        functionCodes_entries.add(new EnumEntry("reservedForMotorController_2", 1));
        functionCodes_entries.add(new EnumEntry("vitalsCommand", 2));
        functionCodes_entries.add(new EnumEntry("warningCode", 3));
        functionCodes_entries.add(new EnumEntry("TelemetryCommand", 4));
        functionCodes_entries.add(new EnumEntry("statusUpdate", 5));
        functionCodes_entries.add(new EnumEntry("HBPing", 6));
        functionCodes_entries.add(new EnumEntry("HBPong", 7));
        functionCodes_entries.add(new EnumEntry("transmitData", 8));
        ENUMS.put("functionCodes", functionCodes_entries);

        List<EnumEntry> vitalsCommands_entries = new ArrayList<>();
        vitalsCommands_entries.add(new EnumEntry("enableContactor", 0));
        vitalsCommands_entries.add(new EnumEntry("disableContactor", 1));
        ENUMS.put("vitalsCommands", vitalsCommands_entries);

        List<EnumEntry> telem_to_vitals_Commands_entries = new ArrayList<>();
        telem_to_vitals_Commands_entries.add(new EnumEntry("lowPowerLora", 0));
        telem_to_vitals_Commands_entries.add(new EnumEntry("highPowerLora", 1));
        telem_to_vitals_Commands_entries.add(new EnumEntry("enablePrechargeIfSafe", 2));
        telem_to_vitals_Commands_entries.add(new EnumEntry("disablePrecharge", 3));
        ENUMS.put("telem_to_vitals_Commands", telem_to_vitals_Commands_entries);

        List<EnumEntry> prechargeState_entries = new ArrayList<>();
        prechargeState_entries.add(new EnumEntry("Off", 0));
        prechargeState_entries.add(new EnumEntry("Precharging", 1));
        prechargeState_entries.add(new EnumEntry("On", 2));
        ENUMS.put("prechargeState", prechargeState_entries);

        List<EnumEntry> TWAI_STATE_entries = new ArrayList<>();
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_STOPPED", 0));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_RUNNING", 1));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_BUS_OFF", 2));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_RECOVERING", 3));
        ENUMS.put("TWAI_STATE", TWAI_STATE_entries);

        List<EnumEntry> vitalsContactorState_entries = new ArrayList<>();
        vitalsContactorState_entries.add(new EnumEntry("allOff", 0));
        vitalsContactorState_entries.add(new EnumEntry("waitingForIntermoduleContactorEnable", 1));
        vitalsContactorState_entries.add(new EnumEntry("waitingForPrechargeContactorEnable", 2));
        vitalsContactorState_entries.add(new EnumEntry("allOn", 3));
        ENUMS.put("vitalsContactorState", vitalsContactorState_entries);

        List<EnumEntry> statusUpdates_entries = new ArrayList<>();
        statusUpdates_entries.add(new EnumEntry("initFlag", 0));
        statusUpdates_entries.add(new EnumEntry("canRecoveryFlag", 1));
        statusUpdates_entries.add(new EnumEntry("canRXOverunFlag", 2));
        statusUpdates_entries.add(new EnumEntry("telemetryCommandAck", 3));
        statusUpdates_entries.add(new EnumEntry("contactorsSuccess", 4));
        statusUpdates_entries.add(new EnumEntry("contactorsFailed", 5));
        ENUMS.put("statusUpdates", statusUpdates_entries);

        List<EnumEntry> dataErrorTrigger_entries = new ArrayList<>();
        dataErrorTrigger_entries.add(new EnumEntry("singleCritical", 0));
        dataErrorTrigger_entries.add(new EnumEntry("extrap4", 1));
        dataErrorTrigger_entries.add(new EnumEntry("extrap8", 2));
        dataErrorTrigger_entries.add(new EnumEntry("confirmedCritical", 3));
        dataErrorTrigger_entries.add(new EnumEntry("enteredWarningRange", 4));
        ENUMS.put("dataErrorTrigger", dataErrorTrigger_entries);

        List<EnumEntry> frameErrorTrigger_entries = new ArrayList<>();
        frameErrorTrigger_entries.add(new EnumEntry("dataTimeout", 0));
        frameErrorTrigger_entries.add(new EnumEntry("repeatedDataTimeout", 1));
        frameErrorTrigger_entries.add(new EnumEntry("resetTimerError", 2));
        ENUMS.put("frameErrorTrigger", frameErrorTrigger_entries);

        List<CommandField> telem_to_vitals_fields = new ArrayList<>();
        telem_to_vitals_fields.add(new CommandField("telem_to_vitals_Commands", 2, 0, 3, "telem_to_vitals_Commands"));
        COMMANDS.add(new Command("telem_to_vitals", 10, 6, false, telem_to_vitals_fields));

        List<CommandField> set_telem_update_frequency_fields = new ArrayList<>();
        set_telem_update_frequency_fields.add(new CommandField("nodeID", 4, 0, 15, null));
        set_telem_update_frequency_fields.add(new CommandField("packet_or_frame_ID", 8, 0, 255, null));
        set_telem_update_frequency_fields.add(new CommandField("divider", 8, 0, 255, null));
        COMMANDS.add(new Command("set_telem_update_frequency", 2, 4, false, set_telem_update_frequency_fields));

        List<CommandField> setChargeCondition_fields = new ArrayList<>();
        setChargeCondition_fields.add(new CommandField("min_MC_Voltage", 8, 80, 335, null));
        setChargeCondition_fields.add(new CommandField("minPercentCharged", 5, 68, 99, null));
        COMMANDS.add(new Command("setChargeCondition", 0, 3, false, setChargeCondition_fields));

        List<CommandField> setCoolantDutyCycle_fields = new ArrayList<>();
        setCoolantDutyCycle_fields.add(new CommandField("dutyCycle", 7, 0, 100, null));
        COMMANDS.add(new Command("setCoolantDutyCycle", 4, 4, false, setCoolantDutyCycle_fields));

        List<CommandField> setCoolantFrequency_HZ_fields = new ArrayList<>();
        setCoolantFrequency_HZ_fields.add(new CommandField("frequency_HZ", 16, 0, 65535, null));
        COMMANDS.add(new Command("setCoolantFrequency_HZ", 12, 4, false, setCoolantFrequency_HZ_fields));

        List<CommandField> forward_packet_fields = new ArrayList<>();
        forward_packet_fields.add(new CommandField("CAN_ID", 11, 0, 2047, null));
        forward_packet_fields.add(new CommandField("dataLength", 4, 0, 15, null));
        forward_packet_fields.add(new CommandField("extendedID", 1, 0, 1, null));
        COMMANDS.add(new Command("forward_packet", 42, 8, true, forward_packet_fields));

    }
}
