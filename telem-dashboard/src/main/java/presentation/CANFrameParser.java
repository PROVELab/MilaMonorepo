/** Auto-generated file. Do not edit. */

package presentation;

public final class CANFrameParser {

    private CANFrameParser() {}

    public interface CANFrameVisitor {
        void visit(IMU_Frame0Packet p);
    }

    public static abstract class ParsedCANFrame {
        public final int nodeId; public final int frameIndex;
        protected ParsedCANFrame(int n, int f) { this.nodeId = n; this.frameIndex = f; }
        public abstract void accept(CANFrameVisitor visitor);
    }

    public static class IMU_Frame0Packet extends ParsedCANFrame {
        public final int[] values;
        public IMU_Frame0Packet(int[] v) { super(9, 0); this.values = v; }
        public int IMU_temp_F() { return values[0]; }
        public int radiator_temp_F() { return values[1]; }
        public int humiditySense_temp_F() { return values[2]; }
        public int RH() { return values[3]; }
        @Override
        public void accept(CANFrameVisitor visitor) { visitor.visit(this); }
    }

    public static ParsedCANFrame createPacket(int nodeId, int frameIndex, int[] values) {
        switch (nodeId) {
            case 9: // IMU
                switch (frameIndex) {
                    case 0: return new IMU_Frame0Packet(values);
                    default: return null;
                }
            default: return null;
        }
    }
}
