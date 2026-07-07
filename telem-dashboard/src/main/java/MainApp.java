import javax.swing.*;

import application.UI.PositionGraphing;
import com.formdev.flatlaf.FlatDarkLaf;

import application.MessageHandler;
import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import com.rinearn.graph3d.RinearnGraph3D;
import lookup.TelemetryLookup;

import java.io.IOException;
import java.io.InputStream;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class MainApp {
    public static void main(String[] args) {
        // Start in dark
        try { UIManager.setLookAndFeel(new FlatDarkLaf()); }
        catch (Exception ex) { System.err.println("Failed to init LaF"); }

        SwingUtilities.invokeLater(() -> {
            try {
                // Generate a timestamped log file name for this session
                LocalDateTime startupTime = LocalDateTime.now();
                DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd_HH-mm-ss");
                String logFileName = "telemLogs" + startupTime.format(formatter) + ".bin";

                // Load Telemetry lookup, which contains info on sensors.
                TelemetryLookup lookup;
                try (InputStream in = MainApp.class.getResourceAsStream("/telemetry.csv")) {
                    if (in == null) throw new IllegalStateException("telemetry.csv not found on classpath");
                    lookup = new TelemetryLookup(in);
                }

                // Build UI
                System.out.println("making UI");
                NotificationPanel notifications = new NotificationPanel();
                SensorSelectionPanel selectionPanel = new SensorSelectionPanel(lookup);
                final int chartCountVertical = 3; final int chartCountHorizontal = 3;
                MainPanel mainPanel = new MainPanel(lookup, chartCountVertical, chartCountHorizontal);
                MainFrame frame = new MainFrame(lookup, selectionPanel, notifications, mainPanel);
                mainPanel.connectFrame(frame);
                frame.setVisible(true);
                PositionGraphing graph3d = new PositionGraphing(new RinearnGraph3D());
                PositionGraphing.setInstance(graph3d);
                System.out.println("parsing");

                // Parse Can Messages, and update UI for them
                MessageHandler parser = new MessageHandler(lookup, notifications, mainPanel, frame, logFileName);

                frame.setVisible(true);

            } catch (IOException e) {
                e.printStackTrace();
                JOptionPane.showMessageDialog(null, "Failed to load telemetry.csv", "Error",
                        JOptionPane.ERROR_MESSAGE);
            }
        });
    }
}
