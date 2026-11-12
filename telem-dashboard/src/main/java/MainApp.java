import javax.swing.*;
import com.formdev.flatlaf.FlatDarkLaf;

import application.MessageHandler;
import application.UI.MainFrame;
import application.UI.MainPanel;
import application.UI.NotificationPanel;
import application.UI.SensorSelectionPanel;
import lookup.TelemetryLookup;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;

import java.io.IOException;
import java.io.InputStream;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;


public class MainApp {
    private static final String DATABASE_URL = "jdbc:sqlite:telem_dashboard.db";
    public static void main(String[] args) {
        // Start in dark
        try { UIManager.setLookAndFeel(new FlatDarkLaf()); }
        catch (Exception ex) { System.err.println("Failed to init LaF"); }
        
         try {
            initializeDatabase();
        } catch (SQLException error) {
            System.err.println("Database setup failed: " + error.getMessage());
            error.printStackTrace();
        }

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
    private static void initializeDatabase() throws SQLException {
        try (Connection db = DriverManager.getConnection(DATABASE_URL);
             var createTable = db.createStatement()) {

            // Create the readings table if this is a new database.
            createTable.executeUpdate("""
                CREATE TABLE IF NOT EXISTS telemetry_readings (
                    session_id TEXT NOT NULL,
                    group_sequence INTEGER NOT NULL,
                    parsed_at_ms INTEGER NOT NULL,
                    elapsed_ns INTEGER NOT NULL,
                    node_id INTEGER NOT NULL,
                    frame_index INTEGER NOT NULL,
                    data_index INTEGER NOT NULL,
                    value INTEGER NOT NULL,
                    PRIMARY KEY (
                        session_id,
                        group_sequence,
                        data_index
                    )
                )
                """);

            // Create the same index used by the logger.
            createTable.executeUpdate("""
                CREATE INDEX IF NOT EXISTS idx_telemetry_session_time
                ON telemetry_readings (session_id, parsed_at_ms)
                """);

            // Create the view used by the guided query.
            // time_seconds is calculated from the stored nanoseconds.
            createTable.executeUpdate("""
                CREATE VIEW IF NOT EXISTS telemetry_readings_seconds AS
                SELECT
                    session_id,
                    group_sequence,
                    parsed_at_ms,
                    elapsed_ns / 1000000000.0 AS time_seconds,
                    node_id,
                    frame_index,
                    data_index,
                    value
                FROM telemetry_readings
                """);
        }
    }
    
}
