package application;

import java.util.Map;
import java.util.NoSuchElementException;
import javax.swing.Timer;

import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;
import lookup.TelemetryRecords;
import presentation.CANFrameParser;
import presentation.GeneratedCANFrameVisitor;

public class DataHandler {
    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;
    private final GeneratedCANFrameVisitor canFrameCallbacks;
    private final Map<TelemetryLookup.FrameKey, Timer> timeoutTimers;

    public DataHandler(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel, GeneratedCANFrameVisitor canFrameCallbacks, Map<TelemetryLookup.FrameKey, Timer> timeoutTimers) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel = mainPanel;
        this.canFrameCallbacks = canFrameCallbacks;
        this.timeoutTimers = timeoutTimers;
    }

    /**
     * The shared logic for processing a fully parsed frame of data values.
     * This method handles timer resets, data validation, plotting, and invoking CAN frame callbacks.
     */
    public void processAndPlotData(int nodeId, int frameIndex, int[] values) {
        if (nodeId == 0 && frameIndex == 0) { // Specific check for HBTiming
            System.out.println("[DataHandler] processAndPlotData started for HBTiming.");
        }

        try {
            TelemetryRecords.CANFrame frame = this.lookup.getFrame(nodeId, frameIndex);

            if (frame.dataTimeout() > 0) { 
                //double + 1 second for telemetry timeout (over vitals timeout). telemetry timeouts seems to need an unfortunate amount of lee-way
                onFrameReceivedResetTimer(nodeId, frameIndex, (frame.dataTimeout()*2) + 1000); 
            }

            if (values.length != frame.numData()) {
                this.notifications.TelemetryUpdate("Mismatched data count for frame. Node: " + nodeId + " Frame: " + frameIndex
                        + ". Expected " + frame.numData() + ", got " + values.length, NotificationPanel.Status.CRITICAL);
                return;
            }

            if (nodeId == 0 && frameIndex == 0) {
                System.out.println("[DataHandler] Looping through " + frame.numData() + " fields for HBTiming...");
            }
            for (int i = 0; i < frame.numData(); i++) {
                TelemetryLookup.DataKey dataKey = new TelemetryLookup.DataKey(nodeId, frameIndex, i);
                TelemetryRecords.DataInfo dataInfo = this.lookup.getDataInfo(dataKey);

                int dataValue = values[i];

                if (nodeId == 0 && frameIndex == 0) System.out.println("  [DataHandler] HBTiming field " + i + ": value=" + dataValue);

                checkDataValue(dataKey, dataInfo, dataValue);
                if (dataInfo.plottable()) {
                    if (nodeId == 0 && frameIndex == 0) System.out.println("    [DataHandler] Plotting HBTiming field " + i);
                    if (!this.mainPanel.addDataPoint(nodeId, frameIndex, i, dataValue)) {
                        this.notifications.TelemetryUpdate("Failed to add data point to main panel. Node: " + nodeId + " Frame: " + frameIndex
                                + " DataIndex: " + i + " Value: " + dataValue, NotificationPanel.Status.WARNING);
                    }
                    if (nodeId == 0 && frameIndex == 0) System.out.println("    [DataHandler] Done plotting HBTiming field " + i);
                }
            }

            if (frame.enableTelemCallback()) {
                CANFrameParser.createPacket(nodeId, frameIndex, values)
                        .ifPresent(canPacket -> canPacket.accept(this.canFrameCallbacks));
            }
        } catch (NoSuchElementException e) {
            String nodeName = this.lookup.getNodeNameOpt(nodeId).orElse("ID " + nodeId);
            String msg = String.format("Failed to process data for %s (frameIndex=%d): Lookup failed. %s",
                                       nodeName, frameIndex, e.getMessage());
            this.notifications.TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
        }
        if (nodeId == 0 && frameIndex == 0) {
            System.out.println("[DataHandler] processAndPlotData finished for HBTiming.");
        }
    }

    private void onFrameReceivedResetTimer(int nodeId, int frameIndex, int timeoutMs) {
        TelemetryLookup.FrameKey key = new TelemetryLookup.FrameKey(nodeId, frameIndex);
        Timer timer = this.timeoutTimers.computeIfAbsent(key, k -> {
            Timer t = new Timer(timeoutMs, e -> onFrameTimeout(k));
            t.setRepeats(false);
            return t;
        });
        timer.restart();
    }

    private void onFrameTimeout(TelemetryLookup.FrameKey key) {
        // Get the expected timeout from the lookup table to make the message more informative.
        try {
            int expectedTimeout = this.lookup.getFrame(key.nodeId(), key.frameIndex()).dataTimeout();
            String nodeName = this.lookup.getNodeName(key.nodeId());
            String msg = String.format("Missing frame: %s (frameIndex=%d). Expected every ~%dms.",
                            nodeName,
                            key.frameIndex(),
                            expectedTimeout);

            this.notifications.TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
        } catch (NoSuchElementException e) {
            this.notifications.TelemetryUpdate("Frame timeout for unknown frame. exception: " + e.getMessage(), NotificationPanel.Status.WARNING);            
        }
    }
    
    private void checkDataValue(TelemetryLookup.DataKey key, TelemetryRecords.DataInfo info, int value) {
        String title = this.lookup.titleFor(key);
        if (value < info.minCritical() || value > info.maxCritical()) {
            String msg = String.format("CRITICAL: %s is %d (out of range [%d, %d])",
                                       title, value, info.minCritical(), info.maxCritical());
            this.notifications.TelemetryUpdate(msg, NotificationPanel.Status.CRITICAL);
        } else if (value < info.minWarning() || value > info.maxWarning()) {
            String msg = String.format("WARNING: %s is %d (out of range [%d, %d])",
                                       title, value, info.minWarning(), info.maxWarning());
            this.notifications.TelemetryUpdate(msg, NotificationPanel.Status.WARNING);
        }
        // Also update the sensor status indicator in the left panel
        SensorSelectionPanel.setStatusIndicator(this.lookup, key, info, value);
    }
}
