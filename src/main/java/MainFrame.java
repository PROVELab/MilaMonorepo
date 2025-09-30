import javax.swing.*;
import javax.swing.border.LineBorder;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import java.awt.*;
import com.formdev.flatlaf.FlatDarkLaf;
import com.formdev.flatlaf.FlatLightLaf;

public class MainFrame extends JFrame {

    private boolean isNightMode = true;
    private final MainPanel mainPanel;

    public void toggleNightMode() {
        try {
            if (isNightMode) {
                UIManager.setLookAndFeel(new FlatLightLaf());
                mainPanel.lightenCharts();
            } else {
                UIManager.setLookAndFeel(new FlatDarkLaf());
                mainPanel.darkenCharts();
            }
            isNightMode = !isNightMode;
            // refresh all windows
            for (Window w : Window.getWindows()) {
                SwingUtilities.updateComponentTreeUI(w);
            }
        } catch (Exception ex) {
            System.out.println("Failed to toggle night mode");
        }
    }

    public Icon getNightModeIcon() {
        String iconName = isNightMode ? "/sun_icon.png" : "/moon_icon.png";
        ImageIcon icon = new ImageIcon(MainFrame.class.getResource(iconName));
        Image image = icon.getImage().getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        return new ImageIcon(image);
    }

    private final JToggleButton addSensorButton = new JToggleButton();

    public boolean getMultiStatus() {
        return addSensorButton.isSelected();
    }

    public MainFrame(
            TelemetryLookup lookup,
            SensorSelectionPanel leftPanel,
            NotificationPanel notifications,
            MainPanel mainPanel
    ) {
        super("Telemetry Dashboard");
        this.mainPanel = mainPanel;
        this.setSize(1000, 700);
        this.setLocationRelativeTo(null);
        this.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        // Left panel (selection) as scrollable
        int leftPanelPreferredWidth = 210;
        leftPanel.setPreferredSize(new Dimension(leftPanelPreferredWidth, 100));
        JScrollPane scrollPane = new JScrollPane(leftPanel);
        scrollPane.setMaximumSize(new Dimension(400, Integer.MAX_VALUE));

        // Notifications width
        int notificationsPreferredWidth = 230;
        notifications.setPreferredSize(new Dimension(notificationsPreferredWidth, 100));

        // Buttons (expand, nightmode, slider, multi)
        JButton expandButton = new JButton("<");
        Dimension buttonSize = new Dimension(37, 37);
        expandButton.setPreferredSize(buttonSize);
        expandButton.setMinimumSize(buttonSize);
        expandButton.setMaximumSize(buttonSize);

        JButton nightModeButton = new JButton(getNightModeIcon());
        nightModeButton.addActionListener(e -> {
            toggleNightMode();
            nightModeButton.setIcon(getNightModeIcon());
        });

        // slider for how many data to show
        JSlider slider = new JSlider(JSlider.HORIZONTAL, 0, 100, 10);
        slider.setMajorTickSpacing(10);
        slider.setMinorTickSpacing(1);
        slider.setPaintTicks(true);
        slider.setPaintLabels(true);
        slider.setPreferredSize(new Dimension(300, 50));
        slider.addChangeListener(new ChangeListener() {
            public void stateChanged(ChangeEvent e) {
                JSlider source = (JSlider) e.getSource();
                if (!source.getValueIsAdjusting()) {
                    mainPanel.setMaxElementsToShow(source.getValue());
                    mainPanel.updateCharts();
                }
            }
        });
        JDialog sliderDialog = new JDialog(this, "Sensor Range");
        sliderDialog.setLayout(new BorderLayout());
        sliderDialog.add(slider, BorderLayout.CENTER);
        sliderDialog.setPreferredSize(new Dimension(500, 100));
        sliderDialog.pack();

        ImageIcon clockIcon = new ImageIcon(MainFrame.class.getResource("/clock.png"));
        Image clockImage = clockIcon.getImage().getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        JButton sliderButton = new JButton(new ImageIcon(clockImage));
        sliderButton.addActionListener(e -> sliderDialog.setVisible(!sliderDialog.isVisible()));
        slider.setPreferredSize(buttonSize);

        ImageIcon multiIcon = new ImageIcon(MainFrame.class.getResource("/multi.png"));
        Image multiImage = multiIcon.getImage().getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        addSensorButton.setIcon(new ImageIcon(multiImage));

        // add the buttons to a panel
        JPanel buttonPanel = new JPanel();
        buttonPanel.setLayout(new BoxLayout(buttonPanel, BoxLayout.Y_AXIS));
        buttonPanel.add(expandButton);
        buttonPanel.add(Box.createVerticalStrut(10));
        buttonPanel.add(nightModeButton);
        buttonPanel.add(Box.createVerticalStrut(10));
        buttonPanel.add(sliderButton);
        buttonPanel.add(Box.createVerticalStrut(10));
        buttonPanel.add(addSensorButton);

        // Splits: (notifications | (left selection | main charts))
        JSplitPane innerSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, scrollPane, mainPanel);
        innerSplit.setDividerLocation(leftPanelPreferredWidth);

        JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, notifications, innerSplit);
        splitPane.setDividerLocation(notificationsPreferredWidth);

        expandButton.addActionListener(e -> {
            if (innerSplit.getDividerLocation() < 50) {
                innerSplit.setDividerLocation(leftPanelPreferredWidth);
                expandButton.setText("<");
            } else {
                innerSplit.setDividerLocation(0);
                expandButton.setText(">");
            }
        });

        // Layout frame
        setLayout(new BorderLayout());
        add(buttonPanel, BorderLayout.WEST);
        add(splitPane, BorderLayout.CENTER);
    }
}
