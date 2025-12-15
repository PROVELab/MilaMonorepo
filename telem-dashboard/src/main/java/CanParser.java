import java.util.Map;
import java.util.Optional;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import javax.swing.SwingUtilities;

//Parses Can messages and updates display. Also formats user messages to Can before sending to telem
public class CanParser {

    private final TelemetryLookup lookup;
    private final NotificationPanel notifications;
    private final MainPanel mainPanel;
    private SerialBridge sb;

    public CanParser(TelemetryLookup lookup, NotificationPanel notifications, MainPanel mainPanel) {
        this.lookup = lookup;
        this.notifications = notifications;
        this.mainPanel=mainPanel;

        System.out.println("Can init");
        final String portName = "/dev/ttyACM0"; final int baud = 115200;
        //read input from Microcontroller
        try {
            this.sb = new SerialBridge(portName, baud, this::onMessageRecv, this::logInvalidFrame);
        } catch (IOException ex) {
            System.out.println("Couldn’t open Microcontroller");
            ex.printStackTrace();
        }

        //Take user commands
        notifications.setOnCommandSubmit(cmd -> {
            buildPayloadFromCommand(cmd).ifPresent(payload -> {
                try { sb.sendMessage(payload); } catch (IOException e) { /* handle */ }
            });
        });

        //Telem monitors HB and CanFrames, to ensure they arent missing
        startHBPongMonitor();
        startCANFrameMonitor();
    }

    //All messages will be 8 bytes of data long. (decided somewhat abritrarily).
    private Optional<byte[]> buildPayloadFromCommand(String input) {
        if (input == null) return Optional.empty();
        input = input.trim();

        // use command with: updateValue<N>
        if (input.startsWith("updateValue")) {
            String numStr = input.substring("updateValue".length()).trim();
            Optional<Integer> parsed = parseInt32(numStr);
            if (parsed.isEmpty()) return Optional.empty();

            int value = parsed.get();
            byte[] msg = new byte[8];
            msg[0] = (byte) (value);
            msg[1] = (byte) (value >>> 8);
            msg[2] = (byte) (value >>> 16);
            msg[3] = (byte) (value >>> 24);

            return Optional.of(msg);
        }
        TelemetryUpdate("unable to interpret provided user command", NotificationPanel.Status.WARNING);
        return Optional.empty();
    }

    private static Optional<Integer> parseInt32(String s) {
        if (s == null || s.isEmpty()) return Optional.empty();
        try { return Optional.of(Integer.parseInt(s.replace("_",""))); // signed decimal
        } catch (NumberFormatException e) {
            return Optional.empty();
        }
    }

    private void logInvalidFrame(byte[] payload) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(payload).order(java.nio.ByteOrder.LITTLE_ENDIAN);
        int id   = bb.getInt();
        long data = bb.getLong();
        final int CanIdMask        = 0b1111111;
        final int functionCodeMask = 0b1111 << 7;
        final int extendedIdMask   = 0x3FFFF << 11;
        final int nodeId       = (id & CanIdMask);
        final int functionCode = (id & functionCodeMask) >> 7;
        final int extendedId   = (id & extendedIdMask)   >> 11;
        System.out.println(String.format(
            " Invalid CAN frame: id=0x%08X func=0x%08X ext=0x%08X data=0x%016X",
            nodeId, functionCode, extendedId, data));
    }

    private void onMessageRecv(byte[] line) {
        if (line == null || line.length != 12) {
            TelemetryUpdate("onMessageRecv got invalid msg. Shouldnt happen (probably Telem code issue)!", NotificationPanel.Status.WARNING);
            return;
        }
        //Assume litte-endian. Arduino is little-endian.
        ByteBuffer bb = ByteBuffer.wrap(line).order(ByteOrder.LITTLE_ENDIAN);
        int id   = bb.getInt();
        long data = bb.getLong();

        parseCanMessage(id, data);        
    }


    private void parseCanMessage(int id, long data) {
        final int CanIdMask = 0b1111111;
        final int functionCodeMask = 0b1111 << 7;
        final int extendedIdMask = 0x3FFFF << 11;

        final int nodeId = (id & CanIdMask);
        final int functionCode = (id & functionCodeMask) >> 7;
        final int extendedId = (id & extendedIdMask) >> 11;
        //Debug message in serial may help for serious issues on display:
        System.out.println(String.format("Got CAN frame id=0x%08X func=0x%08X ext=0x%08X data=0x%016X", nodeId, functionCode, extendedId, data));

        switch (functionCode) {
        case Constants.functionCodes.CAN_Open_NMT_Function:
            parseNMTFunction(nodeId, data);
            break;

        case Constants.functionCodes.CAN_Open_Synchronization:
            parseSynchronization(nodeId, data);
            break;

        case Constants.functionCodes.warningCode:
            
            parseWarningCode(data);
            break;

        case Constants.functionCodes.TelemetryCommand:
            parseTelemCommand(nodeId, data);
            break;

        case Constants.functionCodes.HBPing:
            parseHBPing(nodeId);
            break;

        case Constants.functionCodes.HBPong:
            parseHBPong(nodeId);
            break;

        case Constants.functionCodes.transmitData:
            parseTransmitData(nodeId, extendedId, data);
            break;

        case Constants.functionCodes.HBRespUpdate:
            parseHBRespUpdate(data);
            break;

        case Constants.functionCodes.busStatusUpdate:
            parseBusStatusUpdate(data);
            break;

        case Constants.functionCodes.CAN_Open_Err_Cntrl:
            parseErrorControl(data);
            break;

        case Constants.functionCodes.statusUpdate:
            parseStatusUpdate(nodeId, data);
            break;
        default:
            handleUnknownFunction(functionCode, data);
            break;
        }
    }

    private void parseNMTFunction(int nodeID, long data){
        TelemetryUpdate("Recieved NMT Function Message",
                        NotificationPanel.Status.OK);
    }

    private void parseSynchronization(int nodeID, long data){
            TelemetryUpdate("Recieved Synchronization Message",
                        NotificationPanel.Status.OK);
    }

    //Expects that the flags are followed by nodeId, frameId, dataPoint ID
    private void parseWarningCode(long data) {
        // Currently warning code data < 32 bits.
        final int dataInt = (int) data;

        // Type of warning: (critical or non-critical)
        final int type = dataInt & Constants.warningTypeMask; // 0b111
        final boolean critical = (type & Constants.CriticalWarning) != 0;
        final boolean warning  = (type & Constants.nonCriticalWarning) != 0;

        if (!critical && !warning) {
            TelemetryUpdate("Warning msg with no critical/non-critical type bit set.",
                            NotificationPanel.Status.WARNING);
        } else if (critical && warning) {
            TelemetryUpdate("Warning msg with BOTH critical and non-critical bits set.",
                            NotificationPanel.Status.CRITICAL);
        }

        // ---- Flags string from the entire flags region (bits [0 .. warningNodeFlagIndex-1]) ----
        final int flagsBits  = Math.max(0, Constants.warningNodeFlagIndex);
        final int flagsMask  = (flagsBits == 32) ? -1 : ((1 << flagsBits) - 1);
        final int flagsValue = dataInt & flagsMask;
        final String flagsStr = IntConstUtils.flagsFromInt(Constants.warningFlags.class, flagsValue);

        final NotificationPanel.Status status =
                critical ? NotificationPanel.Status.CRITICAL
                        : NotificationPanel.Status.WARNING;

        // ---- Node ID (after flags region) ----
        final int nodeBits = Math.max(0, Constants.warningFrameFlagIndex - Constants.warningNodeFlagIndex);
        final int nodeMask = (nodeBits == 32) ? -1 : ((1 << nodeBits) - 1);
        final int problematicNode =
                (dataInt >>> Constants.warningNodeFlagIndex) & nodeMask;

        final String nodeName = lookup.getNodeName(problematicNode)
                .orElse("Unrecognized nodeID: " + problematicNode);

        // ---- Frame number ----
        final int frameBits = Math.max(0, Constants.maxFrameCntBits);
        final int frameMask = (frameBits == 32) ? -1 : ((1 << frameBits) - 1);
        final int correspondingFrameNum =
                (dataInt >>> Constants.warningFrameFlagIndex) & frameMask;

        // ---- Data point index ----
        final int dataBits = Math.max(0, Constants.maxDataInFrameBits);
        final int dataMask = (dataBits == 32) ? -1 : ((1 << dataBits) - 1);
        final int correspondingDataPoint =
                (dataInt >>> Constants.warningDataFlagIndex) & dataMask;

        final String dataName = lookup.getDataInfo(problematicNode, correspondingFrameNum, correspondingDataPoint)
                .map(TelemetryLookup.DataInfo::dataName)
                .orElse("unknown dataName");

        final String typeLabel = critical ? "CRITICAL" : (warning ? "WARNING" : "NONE");

        final String message =
                "warning for node: " + nodeName + " (id=" + problematicNode + "). " +
                "type=" + typeLabel + ". flags=" + flagsStr + ". " +
                "Other fields: frame=" + correspondingFrameNum + ", data=" + dataName;

        VitalsUpdate(message, status);
    }


    // frames that indicate responses to telem Commands that we send.
    private void parseTelemCommand(int nodeId, long data){
        final int[] criticalValues = { Constants.telemetryCommandFlags.disablePrecharge };
        final int[] warningValues = {};
        parseEnumFlag(Constants.telemetryCommandFlags.class, nodeId, data,
                    criticalValues, warningValues, NotificationPanel.Channel.TELEMETRY);
    }

    //Indicate we see the Ping Vitals is sending the the HB
    private void parseHBPing(long data) {
        VitalsUpdate("Recieved HB Ping Message",
                        NotificationPanel.Status.OK);
    }

    // Tracks which node IDs have ponged in the current 2s window
    private final java.util.Set<Integer> hbPongWindow = java.util.concurrent.ConcurrentHashMap.newKeySet(); //holds all IDs which have ponged
    private NotificationPanel.Entry HBPongNotification = null;   //Notification to post update
    private Thread hbPongMonitorThread = null;  // Thread for monitoring HB Pongs

    //Responsible for chip depicting how many nodes telem detects responding to HB
    private void startHBPongMonitor() {
        if (hbPongMonitorThread != null) return; // already running

        hbPongMonitorThread = new Thread(() -> {
            while (true) {
                try { Thread.sleep(2000); } catch (InterruptedException ignored) {} //Check every 2s
                final int[] expectedIds = Constants.nodeIDs;
                final int total = expectedIds.length;

                java.util.List<String> missing = new java.util.ArrayList<>();
                int collected = 0;

                for (int id : expectedIds) {
                    if (hbPongWindow.contains(id)) {
                        collected++;
                    } else {
                        var name = lookup.getNodeName(id)
                                        .orElse("id=" + id);
                        missing.add(name);
                    }
                }

                final boolean allOk = (collected == total);
                final NotificationPanel.Status status = allOk
                        ? NotificationPanel.Status.OK
                        : NotificationPanel.Status.WARNING;

                final String msg = allOk
                        ? "HB Pong Status (" + collected + "/" + total + ")"
                        : "HB Pong Status (" + collected + "/" + total + "). Missing: " + String.join(", ", missing);

                SwingUtilities.invokeLater(() -> {
                    if (HBPongNotification == null) {
                        //create new Chip if this is the first time its getting posted
                        HBPongNotification = notifications.post(
                                status, NotificationPanel.Channel.TELEMETRY, msg);
                    } else {
                        HBPongNotification.updateText(notifications, msg);
                        HBPongNotification.updateStatus(status);
                    }
                });

                hbPongWindow.clear(); // start fresh window
            }
        }, "hb-pong-monitor");

        hbPongMonitorThread.setDaemon(true);
        hbPongMonitorThread.start(); 
    }


    private void parseHBPong(int nodeId){
        System.out.println("node Id Pong: " + nodeId);
        var nodeInfoOpt = lookup.getNodeById(nodeId);
        if (nodeInfoOpt.isEmpty()) {
            // Unknown node ID; give warning
            TelemetryUpdate("Received HB Pong from unknown nodeId=" + nodeId, NotificationPanel.Status.WARNING);
            return;
        }

        hbPongWindow.add(nodeId);  // mark for current 2s window
    }

    private void parseTransmitData(int id, int frameIndex, long data) {
        //Extract nodeId and frameIndex from data
        int nodeId = (int) (id & 0b1111111); //
        var frameOpt = lookup.getFrame(nodeId, frameIndex);
        if (frameOpt.isEmpty()) {
            TelemetryUpdate("Transmit Data from unknown nodeId/frameIndex: " + nodeId + "/" + frameIndex
                , NotificationPanel.Status.WARNING);
            return;
        }
        TelemetryLookup.CANFrame frame = frameOpt.get();

        //Update Timeout Tracking
        int timeoutMs = frame.dataTimeout();    //how long before this frame is considered missing
        if (timeoutMs > 0) {    
            onFrameReceivedResetTimer(nodeId, frameIndex, timeoutMs);
        }

        //  Parse the message, and add data to plot. Raise warning if needed
        int bitIndex = 0;
        for(int i=0; i<frame.numData(); i++){
            TelemetryLookup.DataKey dataKey= new TelemetryLookup.DataKey(nodeId, frameIndex, i);
            var dataInfoOpt = lookup.getDataInfo(dataKey);
            if (dataInfoOpt.isEmpty()) {
                TelemetryUpdate("Missing DataInfo for a data indicated to exist by frame's numData value."
                                + "This is an issue with Telemetries lookup, or some node's definition?"
                                + " Node: " + nodeId +" Frame: " + frameIndex + " DataIndex: " + i, 
                                NotificationPanel.Status.WARNING);
                return;
            }
            TelemetryLookup.DataInfo dataInfo = dataInfoOpt.get();
            if(bitIndex + dataInfo.bitLength() > 64){
                TelemetryUpdate("Data overflowed 64 bits. This is an issue with Telemetries lookup"
                                + " Node: " + nodeId +" Frame: " + frameIndex + " DataIndex: " + i, 
                                NotificationPanel.Status.WARNING);
                return;
            }
            if(dataInfo.bitLength() < 0 || dataInfo.bitLength() > 32){
                TelemetryUpdate("DataInfo has invalid bitLength. This is an issue with Telemetries lookup"
                                + " Node: " + nodeId +" Frame: " + frameIndex + " DataIndex: " + i
                                + " bitLength: " + dataInfo.bitLength(), 
                                NotificationPanel.Status.WARNING);
                return;
            }
            long dataMask = (1L << (long) dataInfo.bitLength()) - 1;

            int dataValue = ((int) ((data >> bitIndex) & dataMask));
            dataValue=dataValue + dataInfo.min();   //add the min back
            //Check if the value is out of range. post warning as needed
            checkDataValue(dataKey, dataInfo, dataValue);
            //Add data point to main panel
            if(!mainPanel.addDataPoint(nodeId, frameIndex, i, (int)dataValue)){
                TelemetryUpdate("Failed to add data point to main panel. This is an issue with Telemetries lookup"
                                + " Node: " + nodeId +" Frame: " + frameIndex + " DataIndex: " + i
                                + " Value: " + dataValue, 
                                NotificationPanel.Status.WARNING);
            }
            bitIndex += dataInfo.bitLength();
        }
    }

    // ============= CAN Frame Monitoring =====
    private static final int FRAME_MONITOR_TICK_MS = 5;
    private static final int MISSING_BACKOFF_MIN_MS = 1000; // min reset after a miss


    
// Countdown per frame (mutable); keys come from lookup.framesById()
private final java.util.concurrent.ConcurrentHashMap<TelemetryLookup.FrameKey, Integer> frameRemainingMs =
        new java.util.concurrent.ConcurrentHashMap<>();

private Thread canFrameMonitorThread = null;

/** Call once from ctor. Seeds timers for all frames in lookup. */
private void startCANFrameMonitor() {
    if (canFrameMonitorThread != null) return;

    // Seed all frames up front (only ones with a positive timeout)
    lookup.framesById().forEach((fk, frame) -> {
        if (frame.dataTimeout() > 0) {
            frameRemainingMs.putIfAbsent(fk, frame.dataTimeout());
        }
    });

    canFrameMonitorThread = new Thread(() -> {
        try { Thread.sleep(FRAME_MONITOR_TICK_MS); } catch (InterruptedException ignored) {}

        while (!Thread.currentThread().isInterrupted()) {
            try {
                // Iterate over ALL frames from lookup (not just those we've seen)
                lookup.framesById().forEach((key, frame) -> {
                    final int expected = frame.dataTimeout();
                    if (expected <= 0) return; // not monitored

                    int remaining = frameRemainingMs.getOrDefault(key, expected);
                    int updated   = remaining - FRAME_MONITOR_TICK_MS;

                    if (updated < -FRAME_MONITOR_TICK_MS) { // "< -5ms"
                        final int overdue = -updated;
                        final String nodeStr = lookup.getNodeName(key.nodeId())
                                                     .orElse("id=" + key.nodeId());
                        final String msg = "Missing CAN frame: " + nodeStr
                                + " (frameIndex=" + key.frameIndex() + "). "
                                + "Expected every ~" + expected + "ms; "
                                + "overdue by " + overdue + "ms.";

                        javax.swing.SwingUtilities.invokeLater(() ->
                                TelemetryUpdate(msg, NotificationPanel.Status.WARNING)
                        );

                        // Backoff: after notifying, restart countdown to at least 100ms
                        int resetTo = Math.max(expected, MISSING_BACKOFF_MIN_MS);
                        frameRemainingMs.put(key, resetTo);
                    } else {
                        frameRemainingMs.put(key, updated);
                    }
                });

                Thread.sleep(FRAME_MONITOR_TICK_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (Throwable t) {
                javax.swing.SwingUtilities.invokeLater(() ->
                        TelemetryUpdate("CAN Frame monitor error: " + t.getMessage(),
                                NotificationPanel.Status.WARNING));
            }
        }
    }, "can-frame-monitor");

    canFrameMonitorThread.setDaemon(true);
    canFrameMonitorThread.start();
}

/** Reset a frame’s timer on receipt (call from parseTransmitData after fetching frame). */
private void onFrameReceivedResetTimer(int nodeId, int frameIndex, int timeoutMs) {
    if (timeoutMs > 0) {
        frameRemainingMs.put(new TelemetryLookup.FrameKey(nodeId, frameIndex), timeoutMs);
    }
}

    // ====================Data Monitoring =======================//

    //contains handlers for all dataPoints that have ever been out of range.
    private final Map<TelemetryLookup.DataKey, NotificationPanel.Entry> dataStatusHandlers = new HashMap<>();

    private void checkDataValue(TelemetryLookup.DataKey dataKey, TelemetryLookup.DataInfo dataInfo, int dataValue) {
        NotificationPanel.Entry entry = dataStatusHandlers.get(dataKey);

        boolean inWarning  = dataValue < dataInfo.minWarning()  || dataValue > dataInfo.maxWarning();
        boolean inCritical = dataValue < dataInfo.minCritical() || dataValue > dataInfo.maxCritical();

        NotificationPanel.Status newStatus =
            inCritical ? NotificationPanel.Status.CRITICAL :
            inWarning  ? NotificationPanel.Status.WARNING  :
                        NotificationPanel.Status.OK;

        NotificationPanel.Status oldStatus = (entry != null) ? entry.status : NotificationPanel.Status.OK;
        // if no change in status, do nothing
        if (oldStatus == newStatus) return;

        if (entry == null) {
            // Not tracked before. Lets start tracking it!
            String title = lookup.titleFor(dataKey);
            String msg = "status of: " + title;
            NotificationPanel.Entry newEntry =
                notifications.post(newStatus, NotificationPanel.Channel.TELEMETRY, msg);
            dataStatusHandlers.put(dataKey, newEntry);
        }else{
            //Otherwise, update the existing entry
            entry.updateStatus(newStatus);
        }
    }

    // =================== HB Monitoring  ================= //

    // One timing Notification, and one status entry per possible status frame number.
    private NotificationPanel.Entry HBTimingEntry = null;
    private final NotificationPanel.Entry[] HBStatusEntries =
            new NotificationPanel.Entry[1 << Constants.HBStatusFrameBits];

    // Two type of HB update, timing and status, redirect to the appropriate one
    private void parseHBRespUpdate(long data) {
        final int typeBits = Constants.HBupdateTypeBits;
        assert(typeBits!=0);
        final long typeMask = ((1L << typeBits) - 1L);
        final long typeVal  = data & typeMask;

        if (typeVal == (Constants.HBupdateTiming & typeMask)) {
            parseHBTimingFrame(data);
        } else if (typeVal == (Constants.HBupdateStatus & typeMask)) {
            parseHBStatusFrame(data);
        } else {
            TelemetryUpdate("HB parse: unknown frame type value " + typeVal,
                            NotificationPanel.Status.WARNING);
        }
    }

    //HB Status Frame, which nodes have responded to HB?
    private void parseHBStatusFrame(long data) {
        // Header layout: [ type (HBupdateTypeBits) | frame# (HBStatusFrameBits) | node-bits ... ]
        final int headerBits  = Constants.HBupdateTypeBits + Constants.HBStatusFrameBits;
        final int nodeSlotsPerFrame = 64 - headerBits;
        assert(nodeSlotsPerFrame>30);   //dont want tiny payloads. Reconsider design?

        // Extract status frame number.
        final int frameMask   = (1 << Constants.HBStatusFrameBits) - 1;
        final int frameNo     = (int)((data >>> Constants.HBupdateTypeBits) & frameMask);

        // How many frames are actually used for numberOfNodes?
        final int framesNeeded = ceilDiv(Constants.numberOfNodes, nodeSlotsPerFrame);
        if (frameNo >= framesNeeded) {
            TelemetryUpdate("HB Status parse: received frameNo=" + frameNo +
                            " but framesNeeded=" + framesNeeded +
                            " for numberOfNodes=" + Constants.numberOfNodes,
                            NotificationPanel.Status.WARNING);
            return;
        }

        final int startIndex  = frameNo * nodeSlotsPerFrame;
        final int endIndex    = Math.min(Constants.numberOfNodes, startIndex + nodeSlotsPerFrame);
        if (startIndex >= endIndex) {
            TelemetryUpdate("HB Status parse: empty node range for frame " + frameNo,
                            NotificationPanel.Status.WARNING);
            return;
        }

        // Within this frame, the first node flag is at bit position = headerBits.
        int collected = 0;
        final int expected = endIndex - startIndex;

        StringBuilder missing = new StringBuilder("missingNodes={");
        for (int idx = startIndex; idx < endIndex; idx++) {
            final int bitPos = headerBits + (idx - startIndex);
            final boolean gotHB = ((data >>> bitPos) & 1L) != 0L;
            if (gotHB) {
                collected++;
            } else {
                final int nodeID = Constants.nodeIDs[idx];
                var nodeInfoOpt = lookup.getNodeById(nodeID);
                if (nodeInfoOpt.isPresent()) {
                    missing.append(nodeInfoOpt.get().nodeName()).append(',');
                } else {
                    TelemetryUpdate("HB Status parse: missing nodeInfo for nodeID=" + nodeID,
                                    NotificationPanel.Status.WARNING);
                }
            }
        }
        if (missing.charAt(missing.length() - 1) == ',') {
            missing.setCharAt(missing.length() - 1, '}');
        } else {
            missing.append('}');
        }

        final NotificationPanel.Status st =
                (collected == expected) ? NotificationPanel.Status.OK
                                        : NotificationPanel.Status.WARNING;

        String msg = "HB Status frame " + frameNo + " (" + collected + "/" + expected + ").";
        if (st == NotificationPanel.Status.WARNING) {
            msg += " " + missing;
        }

        final NotificationPanel.Entry entry = HBStatusEntries[frameNo];
        if (entry == null) {
            HBStatusEntries[frameNo] =
                    notifications.post(st, NotificationPanel.Channel.VITALS, msg);
        } else {
            entry.updateText(notifications, msg);
            entry.updateStatus(st);
        }
    }

    // HB Timing Frame. How bad are the worse HB response latencies? Whats the average HB response time?
    private void parseHBTimingFrame(long data) {
        // Layout: [ type (HBupdateTypeBits) | avg (HBTimerMSBits) | (id,nodeMs) * K ... ]
        final int typeBits   = Constants.HBupdateTypeBits;
        final int avgBits    = Constants.HBTimerMSBits;
        final int idBits     = Constants.nodeIDSizeBits;
        final int pairBits   = idBits + Constants.HBTimerMSBits;

        // Extract average.
        int bitIdx = typeBits;
        final long avgMask =  ((1L << avgBits) - 1L);
        final int averageMs = (int)((data >>> bitIdx) & avgMask);
        bitIdx += avgBits;

        // How many (id,time) pairs can actually fit?
        final int availBits = 64 - bitIdx;
        final int maxPairsByBits = (pairBits > 0) ? (availBits / pairBits) : 0;
        final int pairs = Math.min(Constants.slowestNodeCount, Math.max(0, maxPairsByBits));

        StringBuilder details = new StringBuilder("avg=" + averageMs + "ms");
        for (int i = 0; i < pairs; i++) {

            //get the id of the node
            final long idMask = (1L << idBits) - 1L;
            final int nodeID  = (int)((data >>> bitIdx) & idMask);
            if(nodeID == 0){    // a 0 indicates a Null, not enough nodes to fill the frame
                if(Constants.numberOfNodes >= pairs) {  //We shouldnt have any Nulls in HB timing Frame
                    TelemetryUpdate("Got Null node (id=0) for HB timing, even though there are enough nodes to fill the msg."
                        +"This should never happen.", NotificationPanel.Status.WARNING);
                }
                continue;   //ignore Null node
            }
            bitIdx += idBits;   //increment our index in msg

            // get the number of ms to respond
            final long msMask = (1L << Constants.HBTimerMSBits) - 1L;
            final int nodeMs  = (int)((data >>> bitIdx) & msMask);
            bitIdx += Constants.HBTimerMSBits;
            //

            //Add node + response time to notification string.
            details.append("; ");
            var nodeInfoOpt = lookup.getNodeById(nodeID);
            if (nodeInfoOpt.isPresent()) {
                details.append(nodeInfoOpt.get().nodeName()).append('=').append(nodeMs).append("ms");
            } else {
                details.append("id").append(nodeID).append('=').append(nodeMs).append("ms");
                TelemetryUpdate("HB Timing parse: missing nodeInfo for nodeID=" + nodeID,
                                NotificationPanel.Status.WARNING);
            }
        }

        final String msg = "HB Timing: " + details;

        if (HBTimingEntry == null) {
            HBTimingEntry = notifications.post(NotificationPanel.Status.OK,
                                            NotificationPanel.Channel.VITALS, msg);
        } else {
            HBTimingEntry.updateText(notifications, msg);
            HBTimingEntry.updateStatus(NotificationPanel.Status.OK);
        }
    }

    // ================ Parse Bus Status Updates (from Vitals) =========== //
    private NotificationPanel.Entry BusStatusEntry = null;
    private void parseBusStatusUpdate(long data){
        // This is the same format Vitals uses to pack the fields. This could be placed in constants,
        // but this is more convenient (since msg format isnt really a constant).
        final int state          = (int)( data         & ((1L<< 2)-1));
        final int tx_error       = (int)((data >>>  2) & ((1L<< 8)-1));
        final int rx_error       = (int)((data >>> 10) & ((1L<< 8)-1));
        final int bus_err_delta  = (int)((data >>> 18) & ((1L<<12)-1));
        final int tx_fail_delta  = (int)((data >>> 30) & ((1L<<10)-1));
        final int rx_over_delta  = (int)((data >>> 40) & ((1L<<10)-1));
        final int rx_miss_delta  = (int)((data >>> 50) & ((1L<<10)-1));
        final int msgs_to_rx     = (int)((data >>> 60) & ((1L<< 4)-1));

        // Thresholds
        final boolean stateBad = (state != 1); // assumes 1 = RUNNING (see notes below)
        final int errMax = Math.max(tx_error, Math.max(rx_error, bus_err_delta));   //largest err reading we are getting
        final int msgMax = Math.max(Math.max(tx_fail_delta, rx_over_delta), rx_miss_delta); //largest transmission error we are getting
        //Determine the status, based on
        NotificationPanel.Status status;
        if (errMax > 200 || msgMax > 10) {  //Vitals is dropping a lot of stuff
            status = NotificationPanel.Status.CRITICAL;     //This 'critical' doesnt trigger shut off. maybe it should?
        } else if (stateBad || errMax > 100 || msgMax > 0 || msgs_to_rx > 4) {  //warn we are almost dropping/are dropping, or have high error rate
            status = NotificationPanel.Status.WARNING;
        } else {
            status = NotificationPanel.Status.OK;
        }

        // Concise message of all fields
        String msg = "bus: state=" + IntConstUtils.nameFromInt(Constants.twaiState.class, state).orElse("Unknown state value (very bad)")
                + ", txErr=" + tx_error
                + ", rxErr=" + rx_error
                + ", busErrΔ=" + bus_err_delta
                + ", txFailΔ=" + tx_fail_delta
                + ", rxOverΔ=" + rx_over_delta
                + ", rxMissΔ=" + rx_miss_delta
                + ", toRx=" + msgs_to_rx;

        if (BusStatusEntry == null){
            BusStatusEntry = notifications.post(status, NotificationPanel.Channel.VITALS, msg);
        } else {
            BusStatusEntry.updateText(notifications, msg);
            BusStatusEntry.updateStatus(status);
        }
    }

    private void parseErrorControl(long data)    {
        TelemetryUpdate("Recieved CAN Open Error Ctrl Message", NotificationPanel.Status.OK);
    }

    private void parseStatusUpdate(int nodeId, long data){
        final int[] criticalValues = { Constants.statusUpdates.prechargeOff};
        final int[] warningValues = {};
        parseEnumFlag(Constants.statusUpdates.class, nodeId, data,
                 criticalValues, warningValues, NotificationPanel.Channel.TELEMETRY);
    }

    private void handleUnknownFunction(int functionCode, long data) {
        String hexData = Long.toHexString(data);
        TelemetryUpdate("Message with unkown function code observed. function Code = " + functionCode + ". data = " + hexData, NotificationPanel.Status.WARNING);
    }

    //helper to parse flags based on lookup in constants for that flag
    private void parseEnumFlag(Class<?> constantsClass, int nodeId, long data,
            int[] criticalValues, int[] warningValues, NotificationPanel.Channel channel) {

        // Retrieve node info:
        String nodeName = lookup.getNodeName(nodeId)
                .orElse("Unrecognized nodeID: (" + nodeId + ")");
        String nodeInfo = nodeName + " (id=" + nodeId + ")";

        // all enums should fit within an int. check that this code does!
        if (data < Integer.MIN_VALUE || data > Integer.MAX_VALUE) {
            String msg = "parseEnumFlag: value out of int range (" + data + ") from " + nodeInfo;
            SwingUtilities.invokeLater(() ->
                notifications.post(NotificationPanel.Status.WARNING, channel, msg)
            );
            return;
        }

        //Lookup the name of the code
        final int code = (int) data;
        var nameOpt = IntConstUtils.nameFromInt(constantsClass, code);
        if (nameOpt.isEmpty()) {
            String msg = "Unrecognized code " + code + " in "
                    + constantsClass.getSimpleName() + " from " + nodeInfo;
            SwingUtilities.invokeLater(() ->
                notifications.post(NotificationPanel.Status.WARNING, channel, msg)
            );
            return;
        }
        final String constName = nameOpt.get();

        //Determine the status of the messages
        final boolean isCritical = contains(criticalValues, code);
        final boolean isWarning = contains (warningValues, code);
        final NotificationPanel.Status status = isCritical
                ? NotificationPanel.Status.CRITICAL
                : isWarning ? NotificationPanel.Status.WARNING
                : NotificationPanel.Status.OK;

        //Send message:
        final String msg = "Flag=" + constName + " (" + code + ") from " + nodeInfo;
        SwingUtilities.invokeLater(() ->
            notifications.post(status, channel, msg)
        );
    }

    // ================= Helpers ==============//

    private static boolean contains(int[] arr, int v) {
        for (int x : arr) if (x == v) return true;
        return false;
    }

    private static int ceilDiv(int a, int b) {
        if (b <= 0) return 0; 
        return (a + b - 1) / b;
    }

    void TelemetryUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            notifications.post(status,
                NotificationPanel.Channel.TELEMETRY, msg);
        });
    }

    void VitalsUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            notifications.post(status,
                NotificationPanel.Channel.VITALS, msg);
        });
    }

}


