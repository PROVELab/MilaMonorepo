/** Auto-generated file. Do not edit. */

package lookup;

public final class TelemetryRecords {
    private TelemetryRecords() {}

    public record Node(
        int nodeID,
        String nodeName,
        int numFrames
    ) {}

    public record CANFrame(
        int frameIndex,
        int nodeIndex,
        int numData,
        int dataTimeout,
        boolean enableTelemCallback
    ) {}

    public record DataInfo(
        int dataIndex,
        String dataName,
        boolean plottable,
        int min,
        int max,
        int bitLength,
        int minWarning,
        int maxWarning,
        String enumVal,
        int crit_count_max,
        int minCritical,
        int maxCritical
    ) {}

}
