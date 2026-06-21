/** Auto-generated file. Do not edit. */

package presentation;

import java.util.Optional;

public final class CANFrameParser {

    private CANFrameParser() {}

    public interface CANFrameVisitor {
        void visit(IMU_Frame1Packet p);
        void visit(IMU_Frame2Packet p);
        void visit(IMU_Frame3Packet p);
    }

    public static abstract class ParsedCANFrame {
        public final int nodeId; public final int frameIndex;
        protected ParsedCANFrame(int n, int f) { this.nodeId = n; this.frameIndex = f; }
        public abstract void accept(CANFrameVisitor visitor);
    }

    public static class IMU_Frame1Packet extends ParsedCANFrame {
        public final int[] values;
        public IMU_Frame1Packet(int[] v) { super(9, 1); this.values = v; }
        public int posX_m() { return values[0]; }
        public int posY_m() { return values[1]; }
        public int posZ_m() { return values[2]; }
        @Override
        public void accept(CANFrameVisitor visitor) { visitor.visit(this); }
    }

    public static class IMU_Frame2Packet extends ParsedCANFrame {
        public final int[] values;
        public IMU_Frame2Packet(int[] v) { super(9, 2); this.values = v; }
        public int accelX_miliGs() { return values[0]; }
        public int accelY_miliGs() { return values[1]; }
        public int accelZ_miliGs() { return values[2]; }
        @Override
        public void accept(CANFrameVisitor visitor) { visitor.visit(this); }
    }

    public static class IMU_Frame3Packet extends ParsedCANFrame {
        public final int[] values;
        public IMU_Frame3Packet(int[] v) { super(9, 3); this.values = v; }
        public int yaw_degrees() { return values[0]; }
        public int pitch_degrees() { return values[1]; }
        public int roll_degrees() { return values[2]; }
        public int gyroX_deciDegree_p_s() { return values[3]; }
        public int gyroY_deciDegree_p_s() { return values[4]; }
        public int gyroZ_deciDegree_p_s() { return values[5]; }
        @Override
        public void accept(CANFrameVisitor visitor) { visitor.visit(this); }
    }

    public static Optional<ParsedCANFrame> createPacket(int nodeId, int frameIndex, int[] values) {
        switch (nodeId) {
            case 9: // IMU
                switch (frameIndex) {
                    case 1: return Optional.of(new IMU_Frame1Packet(values));
                    case 2: return Optional.of(new IMU_Frame2Packet(values));
                    case 3: return Optional.of(new IMU_Frame3Packet(values));
                    default: return Optional.empty();
                }
            default: return Optional.empty();
        }
    }
}
