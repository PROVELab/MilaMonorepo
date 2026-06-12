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
        ENUMS.put("specialIDs", specialIDs_entries);

        List<EnumEntry> functionCodes_entries = new ArrayList<>();
        functionCodes_entries.add(new EnumEntry("reservedForMotorController_1", 0));
        functionCodes_entries.add(new EnumEntry("reservedForMotorController_2", 1));
        functionCodes_entries.add(new EnumEntry("warningCode", 2));
        functionCodes_entries.add(new EnumEntry("TelemetryCommand", 3));
        functionCodes_entries.add(new EnumEntry("statusUpdate", 4));
        functionCodes_entries.add(new EnumEntry("HBPing", 5));
        functionCodes_entries.add(new EnumEntry("HBPong", 6));
        functionCodes_entries.add(new EnumEntry("transmitData", 7));
        ENUMS.put("functionCodes", functionCodes_entries);

        List<EnumEntry> vitalsCommands_entries = new ArrayList<>();
        vitalsCommands_entries.add(new EnumEntry("lowPowerLora", 0));
        vitalsCommands_entries.add(new EnumEntry("highPowerLora", 1));
        ENUMS.put("vitalsCommands", vitalsCommands_entries);

        List<EnumEntry> prechargeCommands_entries = new ArrayList<>();
        prechargeCommands_entries.add(new EnumEntry("prechargeRemoveLatch", 0));
        prechargeCommands_entries.add(new EnumEntry("prechargeLatchOff", 1));
        prechargeCommands_entries.add(new EnumEntry("prechargeLatchOn", 2));
        ENUMS.put("prechargeCommands", prechargeCommands_entries);

        List<EnumEntry> TWAI_STATE_entries = new ArrayList<>();
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_STOPPED", 0));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_RUNNING", 1));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_BUS_OFF", 2));
        TWAI_STATE_entries.add(new EnumEntry("TWAI_PECAN_RECOVERING", 3));
        ENUMS.put("TWAI_STATE", TWAI_STATE_entries);

        List<EnumEntry> statusUpdates_entries = new ArrayList<>();
        statusUpdates_entries.add(new EnumEntry("initFlag", 0));
        statusUpdates_entries.add(new EnumEntry("canRecoveryFlag", 1));
        statusUpdates_entries.add(new EnumEntry("canRXOverunFlag", 2));
        statusUpdates_entries.add(new EnumEntry("telemetryCommandAck", 3));
        ENUMS.put("statusUpdates", statusUpdates_entries);

        List<EnumEntry> prechargeState_entries = new ArrayList<>();
        prechargeState_entries.add(new EnumEntry("On", 0));
        prechargeState_entries.add(new EnumEntry("Charging", 1));
        prechargeState_entries.add(new EnumEntry("Off", 2));
        ENUMS.put("prechargeState", prechargeState_entries);

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

        List<CommandField> genericVitalsCommand_fields = new ArrayList<>();
        genericVitalsCommand_fields.add(new CommandField("vitalsCommands", 1, 0, 1, "vitalsCommands"));
        COMMANDS.add(new Command("genericVitalsCommand", 24, 7, false, genericVitalsCommand_fields));

        List<CommandField> set_telem_update_frequency_fields = new ArrayList<>();
        set_telem_update_frequency_fields.add(new CommandField("nodeID", 4, 0, 15, null));
        set_telem_update_frequency_fields.add(new CommandField("packet_or_frame_ID", 8, 0, 255, null));
        set_telem_update_frequency_fields.add(new CommandField("divider", 8, 0, 255, null));
        COMMANDS.add(new Command("set_telem_update_frequency", 0, 4, false, set_telem_update_frequency_fields));

        List<CommandField> prechargeCommand_fields = new ArrayList<>();
        prechargeCommand_fields.add(new CommandField("prechargeCommands", 2, 0, 2, "prechargeCommands"));
        COMMANDS.add(new Command("prechargeCommand", 40, 6, false, prechargeCommand_fields));

        List<CommandField> intermoduleCommand_fields = new ArrayList<>();
        intermoduleCommand_fields.add(new CommandField("prechargeCommands", 2, 0, 2, "prechargeCommands"));
        COMMANDS.add(new Command("intermoduleCommand", 8, 6, false, intermoduleCommand_fields));

        List<CommandField> forward_packet_fields = new ArrayList<>();
        forward_packet_fields.add(new CommandField("CAN_ID", 11, 0, 2047, null));
        forward_packet_fields.add(new CommandField("dataLength", 4, 0, 15, null));
        forward_packet_fields.add(new CommandField("extendedID", 1, 0, 1, null));
        COMMANDS.add(new Command("forward_packet", 88, 8, true, forward_packet_fields));

    }
}
