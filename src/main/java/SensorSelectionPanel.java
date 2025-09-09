import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.LineUnavailableException;
import javax.sound.sampled.SourceDataLine;
import javax.sound.sampled.UnsupportedAudioFileException;
import javax.swing.*;
import java.awt.*;
import java.awt.datatransfer.StringSelection;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.ArrayList;
import java.awt.dnd.*;
import java.awt.image.BufferedImage;

record Sensor(String name, String range1, String range2, boolean isCritical, String yLabel) {
    Double[] getRange1(){
        Double d[]  = new Double[2];
        int i = 0;
        for (String s : range1.strip().split("-")){
            d[i++] = Double.parseDouble(s);
        }
        return d;
    }
    Double[] getRange2(){
        Double d[]  = new Double[2];
        int i = 0;
        for (String s : range2.strip().split("-")){
            d[i++] = Double.parseDouble(s);
        }
        return d;
    }
}
public class SensorSelectionPanel extends JPanel {


    private ArrayList<Sensor> sensorList = new ArrayList<>();
    public Sensor[] getSensors(){
        return sensorList.toArray(new Sensor[]{});
    }
    private static HashMap<TelemetryLookup.DataKey, JPanel> sensorStatus = new HashMap<>();

    public SensorSelectionPanel(TelemetryLookup lookup) {
    // Let rows grow to fit however many datapoints you have
    setLayout(new GridLayout(0, 1));

    // One flat iteration over all telemetry channels
    for (TelemetryLookup.DataKey key : lookup.allDataKeys()) {
        // Resolve metadata and a friendly title
        TelemetryLookup.DataInfo dp = lookup.getDataInfoById(key).orElse(null);
        if (dp == null) continue; // CSV could be malformed; skip defensively

        String title = lookup.titleFor(key); // e.g., "<nodeName>.<dataName>"

        // Build the row widget
        JPanel miniElement = new JPanel(new BorderLayout());
        miniElement.setBorder(BorderFactory.createLineBorder(Color.BLACK));

        JLabel sensorLabel = new JLabel(title);
        miniElement.add(sensorLabel, BorderLayout.CENTER);

        // Status indicator (default green)
        JPanel statusIndicator = new JPanel();
        statusIndicator.setPreferredSize(new Dimension(25, 10));
        statusIndicator.setBackground(new Color(76, 175, 80));
        // CHANGED: key the map by the tuple identity
        sensorStatus.put(key, statusIndicator);
        miniElement.add(statusIndicator, BorderLayout.EAST);

        // Helpful tooltip
        miniElement.setToolTipText(
            "nodeId=" + key.nodeId() + ", frameIdx=" + key.frameIndex() + ", dpIdx=" + key.dataIndex()
        );

        // Drag setup (Variant B with drag image)
        final int nodeId  = key.nodeId();
        final int frameIdx = key.frameIndex();
        final int dpIdx    = key.dataIndex();
        final String nodeName = lookup.getNodeById(nodeId).map(TelemetryLookup.Node::name).orElse("node" + nodeId);

        DragSource ds = new DragSource();
        ds.createDefaultDragGestureRecognizer(sensorLabel, DnDConstants.ACTION_COPY, dge -> {
            try {
                // Render the miniElement as a drag image (optional but nice)
                BufferedImage img = new BufferedImage(
                    miniElement.getWidth(), miniElement.getHeight(), BufferedImage.TYPE_INT_ARGB
                );
                Graphics2D g2 = img.createGraphics();
                miniElement.printAll(g2);
                g2.dispose();

                Image dragImage = Toolkit.getDefaultToolkit().createImage(img.getSource());
                Point dragOffset = new Point(0, 0);

                // Your DnD payload should carry nodeId/frameIdx/dpIdx
                DataInfoRef ref  = new DataInfoRef(nodeName, nodeId, frameIdx, dpIdx);
                DataInfoTransferable xfer = new DataInfoTransferable(ref);

                ds.startDrag(dge, DragSource.DefaultCopyDrop, dragImage, dragOffset, xfer, new DragSourceAdapter(){});
            } catch (Exception ex) {
                ex.printStackTrace();
            }
        });

        // Add the new element to the panel!
        add(miniElement);
    }
}



    //Setter method to change the status indicator of the sensors. key = nodeName + "." + dataInfo.dataName()
    public static void setStatusIndicator(TelemetryLookup lookup, TelemetryLookup.DataKey key, TelemetryLookup.DataInfo dataInfo, double value) {
        // Normal (green): within [min, max]
        boolean inNormal = value >= dataInfo.min() && value <= dataInfo.max();

        // Warning (yellow): within [minWarning, maxWarning]
        boolean inWarn = value >= dataInfo.minWarning() && value <= dataInfo.maxWarning();

        // Critical (red): outside warning band; optionally use critical band for alert
        boolean inCriticalBand = value < dataInfo.minCritical() || value > dataInfo.maxCritical();

        JPanel indicator = sensorStatus.get(key);
        if (indicator == null) return; // nothing to color

        if (inNormal) {
            indicator.setBackground(new Color(76, 175, 80));       // green
        } else if (inWarn) {
            indicator.setBackground(new Color(255, 235, 59));      // yellow
        } else {
            indicator.setBackground(new Color(244, 67, 54));       // red

            // Optional: show an alert when outside the critical band
            if (inCriticalBand) {
                SwingUtilities.invokeLater(() ->
                    JOptionPane.showMessageDialog(
                        null,
                        lookup.titleFor(key) + " is in critical range!",
                        "Critical Alert",
                        JOptionPane.ERROR_MESSAGE
                    )
                );

                // Optional sound (kept from your original). Safe-guarded and EDT-friendly.
                new Thread(() -> {
                    try (var audioInputStream =
                            AudioSystem.getAudioInputStream(new File("resources/alert.wav").getAbsoluteFile())) {

                        AudioFormat format = audioInputStream.getFormat();
                        try (SourceDataLine line = AudioSystem.getSourceDataLine(format)) {
                            line.open(format);
                            line.start();
                            byte[] buffer = new byte[4096];
                            int bytesRead;
                            while ((bytesRead = audioInputStream.read(buffer)) != -1) {
                                line.write(buffer, 0, bytesRead);
                            }
                            line.drain();
                        }
                    } catch (UnsupportedAudioFileException | IOException | LineUnavailableException ex) {
                        ex.printStackTrace();
                    }
                }, "alert-sound").start();
            }
        }
    }

}
