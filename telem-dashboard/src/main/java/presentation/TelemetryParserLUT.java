package presentation;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Optional;
import java.util.Comparator;
import java.util.List;
import presentation.BitStream;
public final class TelemetryParserLUT { // Auto-generated. Do not edit.

    private TelemetryParserLUT() {} // Private constructor to prevent instantiation

    public interface PacketVisitor {
        void visit(VitalsUpdatePacket p);
        void visit(vitalsErrPacket p, BitStream stream);
        void visit(dataWarningPacket p);
        void visit(frameWarningPacket p);
        void visit(nodeStatusPacket p);
        void visit(unknownCanPacketPacket p, BitStream stream);
        void visit(CANDataFramePacket p, BitStream stream);
        void visit(ParsedPacket p); // Fallback for unhandled packets
    }

    public static abstract class ParsedPacket {
        public final String packetName;
        public int packetIndex;
        protected ParsedPacket(String name) { this.packetName = name; }
        public abstract void accept(PacketVisitor visitor, BitStream stream);
        public abstract int[] getValues();
        @Override
        public String toString() { return packetName; }
    }

    public static final class ParseResult {
        public final Optional<ParsedPacket> packet;
        public final int bytesConsumed;
        public ParseResult(Optional<ParsedPacket> p, int b) { packet = p; bytesConsumed = b; }
    }

    public static class VitalsUpdatePacket extends ParsedPacket {
        public final int[] values = new int[17];
        public VitalsUpdatePacket() { super("VitalsUpdate"); }
        public int vitalsContactorState() { return values[0]; }
        public int contactorStateLatched() { return values[1]; }
        public int TWAI_TX_Err_Cnt() { return values[2]; }
        public int TWAI_RX_Err_Cnt() { return values[3]; }
        public int TWAI_Err_Cnt() { return values[4]; }
        public int failed_TX_Cnt() { return values[5]; }
        public int RX_Overrun_Cnt() { return values[6]; }
        public int RX_Missed_Cnt() { return values[7]; }
        public int RX_Recv_Queue_Cnt() { return values[8]; }
        public int slowestNode1_ID() { return values[9]; }
        public int slowestNode1_time() { return values[10]; }
        public int slowestNode2_ID() { return values[11]; }
        public int slowestNode2_time() { return values[12]; }
        public int slowestNode3_ID() { return values[13]; }
        public int slowestNode3_time() { return values[14]; }
        public int TWAI_STATE() { return values[15]; }
        public int HBMask() { return values[16]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this);
        }
    }

    public static class vitalsErrPacket extends ParsedPacket {
        public final int[] values = new int[1];
        public byte[] payload;
        public vitalsErrPacket() { super("vitalsErr"); }
        public int numErrors() { return values[0]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this, stream);
        }
    }

    public static class dataWarningPacket extends ParsedPacket {
        public final int[] values = new int[5];
        public dataWarningPacket() { super("dataWarning"); }
        public int dataTooHigh() { return values[0]; }
        public int dataErrorTrigger() { return values[1]; }
        public int nodeID() { return values[2]; }
        public int frameID() { return values[3]; }
        public int dataID() { return values[4]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this);
        }
    }

    public static class frameWarningPacket extends ParsedPacket {
        public final int[] values = new int[3];
        public frameWarningPacket() { super("frameWarning"); }
        public int frameErrorTrigger() { return values[0]; }
        public int nodeID() { return values[1]; }
        public int frameID() { return values[2]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this);
        }
    }

    public static class nodeStatusPacket extends ParsedPacket {
        public final int[] values = new int[2];
        public nodeStatusPacket() { super("nodeStatus"); }
        public int nodeID() { return values[0]; }
        public int statusUpdates() { return values[1]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this);
        }
    }

    public static class unknownCanPacketPacket extends ParsedPacket {
        public final int[] values = new int[5];
        public byte[] payload;
        public unknownCanPacketPacket() { super("unknownCanPacket"); }
        public int nodeID() { return values[0]; }
        public int DLC() { return values[1]; }
        public int extendedIDPresent() { return values[2]; }
        public int RTR() { return values[3]; }
        public int ext_id_start() { return values[4]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this, stream);
        }
    }

    public static class CANDataFramePacket extends ParsedPacket {
        public final int[] values = new int[2];
        public byte[] payload;
        public CANDataFramePacket() { super("CANDataFrame"); }
        public int nodeID() { return values[0]; }
        public int frameID() { return values[1]; }
        @Override
        public int[] getValues() { return values; }
        @Override
        public void accept(PacketVisitor visitor, BitStream stream) {
            visitor.visit(this, stream);
        }
    }

    static class LutEntry {
        public final int mask, bits, packetIndex;
        public final String name;
        public final boolean isCustom;
        public final PacketCreator creator;
        public LutEntry(int m, int b, String n, boolean c, int pIdx, PacketCreator creator) { mask=m; bits=b; name=n; isCustom=c; packetIndex=pIdx; this.creator = creator; }
    }

    static final List<LutEntry> LUT = new ArrayList<>();

    static {
        LUT.add(new LutEntry(0x2, 2, "VitalsUpdate", false, 0, VitalsUpdatePacket::new));
        LUT.add(new LutEntry(0x11, 8, "vitalsErr", true, 1, vitalsErrPacket::new));
        LUT.add(new LutEntry(0x191, 10, "dataWarning", false, 2, dataWarningPacket::new));
        LUT.add(new LutEntry(0x391, 16, "frameWarning", false, 3, frameWarningPacket::new));
        LUT.add(new LutEntry(0x91, 9, "nodeStatus", false, 4, nodeStatusPacket::new));
        LUT.add(new LutEntry(0x1, 5, "unknownCanPacket", true, 5, unknownCanPacketPacket::new));
        LUT.add(new LutEntry(0x0, 2, "CANDataFrame", true, 6, CANDataFramePacket::new));
        LUT.sort(Comparator.comparingInt((LutEntry e) -> e.bits).thenComparingInt(e -> e.mask)); // Sort by length, then mask for a stable, predictable order.
    }

    @FunctionalInterface
    interface PacketCreator { TelemetryParserLUT.ParsedPacket create(); }

}
