import javax.swing.*;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import java.awt.*;
import java.io.IOException;
import java.io.InputStream;

public class MainFrame extends JFrame {

    private static JToggleButton addSensorButton = new JToggleButton();

    public MainFrame() {
        //Title for App
        super("Telemetry Dashboard");
        
        //Loads the telemetry CSV, creating lookup for sensor info
        TelemetryLookup lookup = null;
        try (InputStream in = MainApp.class.getResourceAsStream("/telemetry.csv")) {
            if (in == null) throw new IllegalStateException("telemetry.csv not found");
            lookup = new TelemetryLookup(in);   // parse CSV into memory
        }catch (IOException e) {
            e.printStackTrace();
            // maybe handle it more gracefully, e.g. show an error dialog
        }
        // At this point, `in` is closed, but `tl` is still usable
        //lookup.getFrameById(0, 0).ifPresent(f -> System.out.println(f.dataTimeout()));

        // Left Panel with Scrollbar
        SensorSelectionPanel leftPanel = new SensorSelectionPanel(lookup);
        Sensor[] sensors = leftPanel.getSensors();
        leftPanel.setPreferredSize(new Dimension(210, 100)); // Set preferred width to 400
        int leftPanelPreferredWidth = 210;
        JScrollPane scrollPane = new JScrollPane(leftPanel);
        scrollPane.setMaximumSize(new Dimension(400, Integer.MAX_VALUE)); // Set maximum width to 400
    
        // NEW — Notification panel (left-most)
        NotificationPanel notifications = new NotificationPanel();
        int notificationsPreferredWidth = 230;
        notifications.setPreferredSize(new Dimension(notificationsPreferredWidth, 100));

        //Create Can Parse to handle Can messages
        CanParser parser = new CanParser();
        // --- Sample posts (simple “testing”) ---
        //This temporary test code will go into Can Parser.
        // var hVitalsOk  = notifications.post(NotificationPanel.Status.OK,
        //                                     NotificationPanel.Channel.VITALS,
        //                                     "Vitals online");
        // var hTelemWarn = notifications.post(NotificationPanel.Status.WARNING,
        //                                     NotificationPanel.Channel.VITALS,
        //                                     "Frame 0 latency observed more random crap");
        // for (int i=0;i<30;i++){
        //     String dumby="";
        //     for (int j=0;j<i;j++){
        //         dumby+=i+" ";
        //     }
        //             var notif = notifications.post(NotificationPanel.Status.WARNING,
        //                                     NotificationPanel.Channel.VITALS,
        //                                     "Notification "+dumby);
        // }


        // Optional: later update (demonstrates handle usage; safe even if user dismisses)
        SwingUtilities.invokeLater(() ->
            hTelemWarn.updateText("Frame 0 latency observed more random crap")
        );

        // Main Panel with Graph Components
        final int chartCountVertical = 2;
        final int chartCountHorizontal = 2;
        MainPanel mainPanel = new MainPanel(lookup, chartCountVertical, chartCountHorizontal);
    
        // Create expand/collapse button for the Left Panel
        JButton expandButton = new JButton("<");
        Dimension buttonSize = new Dimension(37, 37); // Set button size
        expandButton.setPreferredSize(buttonSize);
        expandButton.setMinimumSize(buttonSize);
        expandButton.setMaximumSize(buttonSize);

        // Create a button to toggle night mode
        JButton nightModeButton = new JButton(MainApp.getNightModeIcon());
        nightModeButton.addActionListener(e -> {
            MainApp.toggleNightMode();
            nightModeButton.setIcon(MainApp.getNightModeIcon()); // Update the button icon
        });

        // Add the button to the frame
        add(nightModeButton, BorderLayout.NORTH);
    
        // Create a panel for the button with a flow layout aligned to the right
        JPanel buttonPanel = new JPanel();
        buttonPanel.setLayout(new BoxLayout(buttonPanel, BoxLayout.Y_AXIS));

        //Add the buttons to the button panel
        buttonPanel.add(expandButton);
        buttonPanel.add(Box.createVerticalStrut(10)); // Add some space between the buttons
        buttonPanel.add(nightModeButton);
        buttonPanel.add(Box.createVerticalStrut(10)); // Add some space between the buttons
    
        // innermost split: Sensor Selection | Main
        JSplitPane innerSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, scrollPane, mainPanel);
        innerSplit.setDividerLocation(leftPanelPreferredWidth);

        // outer split: Notifications | (inner split)
        JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, notifications, innerSplit);
        splitPane.setDividerLocation(notificationsPreferredWidth);

        //Logic for Expand/Collapse button for innermost split
        expandButton.addActionListener(e -> {
            if (innerSplit.getDividerLocation() < 50) {
                innerSplit.setDividerLocation(leftPanelPreferredWidth); // Expand Sensor Selection
                expandButton.setText("<");
            } else {
                innerSplit.setDividerLocation(0); // Collapse Sensor Selection
                expandButton.setText(">");
            }
        });

        // Create the slider
        JSlider slider = new JSlider(JSlider.HORIZONTAL, 0, 100, 10);
        slider.setMajorTickSpacing(10);
        slider.setMinorTickSpacing(1);
        slider.setPaintTicks(true);
        slider.setPaintLabels(true);
        slider.setPreferredSize(new Dimension(300, 50)); // Set a preferred size for the slider

        slider.addChangeListener(new ChangeListener() {
            public void stateChanged(ChangeEvent e) {
                JSlider source = (JSlider)e.getSource();
                if (!source.getValueIsAdjusting()) {
                    MainPanel.setMaxElementsToShow((MainPanel.getMaxElementsToShow() + source.getValue() > 100)  ? 100: MainPanel.getMaxElementsToShow() + source.getValue());
                    MainPanel.updateCharts();
                }
            }
        });

        // Create a JDialog to act as a pop-up panel for the slider
        JDialog dialog = new JDialog();
        dialog.setTitle("Sensor Range");
        dialog.setLayout(new BorderLayout());
        dialog.add(slider, BorderLayout.CENTER); // Add the slider to the dialog
        dialog.setPreferredSize(new Dimension(500,100));
        dialog.pack();

        //Setting the icon for the Clock
        ImageIcon icon = new ImageIcon(
            MainApp.class.getResource("/clock.png")  // leading "/" means root of resources
        );
        Image image = icon.getImage();
        Image newImage = image.getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        icon = new ImageIcon(newImage);
        
        // Create Button for the Clock
        JButton sliderButton = new JButton(icon);
        sliderButton.addActionListener(e -> {
            dialog.setVisible(!dialog.isVisible()); // Toggle the visibility of the dialog
        });
        slider.setPreferredSize(buttonSize);

        //Set the icon for multi
        ImageIcon multiIcon = new ImageIcon(
            MainApp.class.getResource("/multi.png")  // leading "/" means root of resources
        );
        Image multiImage = multiIcon.getImage();
        Image newMultiImage = multiImage.getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        multiIcon = new ImageIcon(newMultiImage);
        addSensorButton.setIcon(multiIcon);

        // Add the button to the button panel
        buttonPanel.add(sliderButton);
        buttonPanel.add(Box.createVerticalStrut(10)); // Add some space between the buttons
        //Add the the multi button
        buttonPanel.add(addSensorButton);

        // Create a panel for the split pane and the button
        JPanel splitPanePanel = new JPanel(new BorderLayout());
        splitPanePanel.add(buttonPanel, BorderLayout.WEST);
        splitPanePanel.add(splitPane, BorderLayout.CENTER);
    
        // Set layout and add split pane panel to the frame
        setLayout(new BorderLayout());
        add(splitPanePanel, BorderLayout.CENTER);
    }

    public static boolean getMultiStatus(){
        if (addSensorButton.isSelected()){
            return true;
        }
        else{
            return false;
        }
    }
    
}

