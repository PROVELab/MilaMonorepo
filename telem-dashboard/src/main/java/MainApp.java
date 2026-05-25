import javax.swing.*;
import com.formdev.flatlaf.FlatDarkLaf;

import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;

import java.io.IOException;
import java.io.InputStream;

public class MainApp {
    public static void main(String[] args) {
        // Start in dark
        try { UIManager.setLookAndFeel(new FlatDarkLaf()); }
        catch (Exception ex) { System.err.println("Failed to init LaF"); }

        SwingUtilities.invokeLater(() -> {
            try {
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
                final int chartCountVertical = 2; final int chartCountHorizontal = 2;
                MainPanel mainPanel = new MainPanel(lookup, chartCountVertical, chartCountHorizontal);
                MainFrame frame = new MainFrame(lookup, selectionPanel, notifications, mainPanel);
                mainPanel.connectFrame(frame);
                frame.setVisible(true);
                System.out.println("parsing");

                // Parse Can Messages, and update UI for them
                CanParser parser = new CanParser(lookup, notifications, mainPanel, frame);

                frame.setVisible(true);

            } catch (IOException e) {
                e.printStackTrace();
                JOptionPane.showMessageDialog(null, "Failed to load telemetry.csv", "Error",
                        JOptionPane.ERROR_MESSAGE);
            }
        });
    }
}
