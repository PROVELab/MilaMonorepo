package application.UI;
import javax.swing.*;
import javax.swing.border.TitledBorder;
import java.awt.*;
import java.util.*;
import java.util.List;
import java.util.function.Consumer;

public class NotificationPanel extends JPanel {

    // specification for notifications: 
    public enum Channel { VITALS, TELEMETRY }
    public enum Status  { OK, WARNING, CRITICAL }


    // track all live Entry objects so we can find/close matches
    private final List<Entry> entries = new ArrayList<>();
    // Ensure we dont start spamming the same msg if sent repeatedly. 

    // Used as a handler for each notification 
    public static final class Entry {
        final NotificationPanel owner;

        final Channel channel;
        Status status;
        final JPanel chip;
        final JTextArea text;
        final JButton expandBtn;    //arrow to expand text box;
        final JButton closeBtn;     //X to close notifications
        final JPanel square;        //square the message is stored on
        final JLabel ageLabel;     // Indicates age of message in seconds
        boolean expanded = false;
        boolean posted = true;     // track whether still visible
        boolean oneActive = false; // whether this chip’s “1” is active (underlined)
        int ageSeconds = 0; 

        Entry(NotificationPanel owner, Channel ch, Status st, JPanel chip, JTextArea text,
          JButton expandBtn, JButton closeBtn, JPanel square, JLabel ageLabel) {
        this.owner = owner;
        this.channel = ch; this.status = st;
        this.chip = chip; this.text = text; this.expandBtn = expandBtn;
        this.closeBtn = closeBtn; this.square = square;
        this.ageLabel = ageLabel;
    }

    // updateText of notification, reposts if it was closed
    public void updateText(NotificationPanel ownerParam, String newText) {
        SwingUtilities.invokeLater(() -> {
            NotificationPanel p = (ownerParam != null) ? ownerParam : owner;
            if (!posted) {
                p.rePostEntry(this);
            }
            text.setText(newText);
            Section section = (channel == Channel.VITALS) ? p.vitals : p.telemetry;
            if (expanded) p.applyExpandedSize(this, section);
            chip.revalidate();
            chip.repaint();
        });
    }

    // updateStatus of notification, reposts if it was closed
    public void updateStatus(Status newStatus) {
        SwingUtilities.invokeLater(() -> {
            if (!posted) {
                owner.rePostEntry(this); 
            }
            this.status = newStatus;
            square.setBackground(NotificationPanel.colorFor(newStatus));
            chip.revalidate();
            chip.repaint();
        });
    }

    //for when the user clicks the close button
    public void unPostEntry(NotificationPanel owner) {
        SwingUtilities.invokeLater(() -> {
            if (!posted) return;
            posted = false;
            owner.unPostEntry(this);
        });
    }

    }

    public Entry post(Status status, Channel channel, String text) {
        return createChip(status, channel, text);
    }

    // Helper to add an existing Entry back into the UI
    private void rePostEntry(Entry e) {
        if (e.posted) return;
        e.posted = true;

        Section section = (e.channel == Channel.VITALS) ? vitals : telemetry;
        section.list.add(e.chip, 0);
        if (!entries.contains(e)) entries.add(e);

        if (!e.expanded) applyCollapsedSize(e);

        section.list.revalidate();
        section.list.repaint();
    }


    // Layout constants
    private static final int CHIP_HEIGHT = 30;
    private static final int CHIP_VPAD   = 6;
    private static final int GAP         = 6;

    // Section (top/bottom)
    private static final class Section {
        final JPanel root;
        final JPanel list;
        final JScrollPane scroller;

        Section(String title) {
            root = new JPanel(new BorderLayout());
            root.setBorder(BorderFactory.createTitledBorder(
                    BorderFactory.createLineBorder(Color.GRAY),
                    title, TitledBorder.LEFT, TitledBorder.TOP));

            list = new JPanel();
            list.setLayout(new BoxLayout(list, BoxLayout.Y_AXIS));
            list.setOpaque(false);
            list.setAlignmentX(Component.LEFT_ALIGNMENT);

            scroller = new JScrollPane(list,
                    ScrollPaneConstants.VERTICAL_SCROLLBAR_AS_NEEDED,
                    ScrollPaneConstants.HORIZONTAL_SCROLLBAR_NEVER);
            scroller.setBorder(BorderFactory.createEmptyBorder());
            scroller.getViewport().setAlignmentY(0f);
            root.add(scroller, BorderLayout.CENTER);
        }
    }

    // Notification sections for vitals and telem
    private final Section vitals    = new Section("Vitals Notifications");
    private final Section telemetry = new Section("Telemetry Notifications");

    private final JPanel stacked;
    private final javax.swing.Timer ageTicker;

    //The panel holding all the notifications
    public NotificationPanel() {
        super(new BorderLayout());
        setPreferredSize(new Dimension(260, 0));

        stacked = new JPanel();
        stacked.setLayout(new GridLayout(2, 1, 0, 6));
        stacked.add(vitals.root);
        stacked.add(telemetry.root);

        add(stacked, BorderLayout.CENTER);

        // Increment all chips seconds counter every second
        ageTicker = new javax.swing.Timer(1000, ae -> {
            for (Entry e : new ArrayList<>(entries)) { // copy to avoid concurrent modification
                if (e.posted) {
                    e.ageSeconds += 1;
                    e.ageLabel.setText(e.ageSeconds + "s");
                }
            }
        });
        ageTicker.start();
    }

    public void setCommandPanel(JPanel commandPanel) {
        // The 'stacked' panel is already in the CENTER of this panel's BorderLayout.
        // We will replace it with a JSplitPane to make the sections resizable.
        this.remove(stacked);

        JSplitPane verticalSplit = new JSplitPane(JSplitPane.VERTICAL_SPLIT, stacked, commandPanel);
        verticalSplit.setResizeWeight(0.7); // Give 70% to notifications, 30% to commands initially
        verticalSplit.setBorder(BorderFactory.createEmptyBorder()); // Looks cleaner

        this.add(verticalSplit, BorderLayout.CENTER);
        this.revalidate();
        this.repaint();
    }

    //create a new chip (notification) to put on panel
    private Entry createChip(Status status, Channel channel, String msg) {

        //if chip not already present, make one!
        JPanel chip = new JPanel(new BorderLayout(GAP, 0));
        chip.setBorder(BorderFactory.createCompoundBorder(
                BorderFactory.createEmptyBorder(4, 8, 4, 8),
                BorderFactory.createLineBorder(new Color(0,0,0,40))
        ));
        chip.setOpaque(false);
        chip.setAlignmentX(Component.LEFT_ALIGNMENT);

        // Left side status square
        JPanel square = new JPanel();
        square.setPreferredSize(new Dimension(14, 14));
        square.setMaximumSize(new Dimension(14, 14));
        square.setBackground(colorFor(status));
        square.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY, 1));

        JPanel squareWrap = new JPanel(new GridBagLayout());
        squareWrap.setOpaque(false);
        squareWrap.add(square);
        chip.add(squareWrap, BorderLayout.WEST);

        // CENTER: JTextArea
        JTextArea text = new JTextArea(msg);
        text.setLineWrap(true);
        text.setWrapStyleWord(true);
        text.setEditable(false);
        text.setOpaque(false);
        text.setBorder(BorderFactory.createEmptyBorder());
        chip.add(text, BorderLayout.CENTER);

        // Buttons on the Right of chip
        JButton expand = new JButton("▸");
        stylizeMiniButton(expand);
        expand.setToolTipText("Expand/collapse");

        JButton close = new JButton("×");
        stylizeMiniButton(close);
        close.setForeground(Color.RED.darker());
        close.setToolTipText("Dismiss");

        JLabel age = new JLabel("0s");           // live age indicator
        age.setToolTipText("Seconds since posted/updated");

        JPanel right = new JPanel();
        right.setOpaque(false);
        right.setLayout(new BoxLayout(right, BoxLayout.X_AXIS));
        right.add(expand);
        right.add(Box.createHorizontalStrut(4));
        right.add(age);    
        right.add(Box.createHorizontalStrut(4));
        right.add(close);
        chip.add(right, BorderLayout.EAST);
        //

        // create the entry
        Entry entry = new Entry(this, channel, status, chip, text, expand, close, square, age);

        Section section = (channel == Channel.VITALS) ? vitals : telemetry;
        section.list.add(chip, 0);

        applyCollapsedSize(entry);
        expand.setVisible(true);

        //add listeners for the buttons
        // expansion button
        expand.addActionListener(ae -> {
            entry.expanded = !entry.expanded;
            expand.setText(entry.expanded ? "▾" : "▸");
            if (entry.expanded) applyExpandedSize(entry, section);
            else applyCollapsedSize(entry);
            section.list.revalidate();
            section.list.repaint();
        });

        //X button
        close.addActionListener(ae -> {
            unPostEntry(entry);
        });

        //add the chip to panel
        entries.add(entry); 
        section.list.revalidate();
        section.list.repaint();
        return entry;
    }


    private void stylizeMiniButton(JButton b) {
        b.setMargin(new Insets(1, 6, 1, 6));
        b.setFocusPainted(false);
        b.setBorder(BorderFactory.createEmptyBorder());
        b.setContentAreaFilled(false);
    }

    private void applyCollapsedSize(Entry e) {
        e.chip.setMinimumSize(new Dimension(0, CHIP_HEIGHT));
        e.chip.setPreferredSize(new Dimension(0, CHIP_HEIGHT));
        e.chip.setMaximumSize(new Dimension(Integer.MAX_VALUE, CHIP_HEIGHT));
    }

    private void applyExpandedSize(Entry e, Section section) {
        Dimension pref = e.text.getPreferredSize();
        int target = Math.max(CHIP_HEIGHT, pref.height + CHIP_VPAD * 2);
        e.chip.setMinimumSize(new Dimension(0, target));
        e.chip.setPreferredSize(new Dimension(0, target));
        e.chip.setMaximumSize(new Dimension(Integer.MAX_VALUE, target));
        e.text.revalidate();
        e.chip.revalidate();
    }

    private void unPostEntry(Entry e) {
        e.posted = false;
        Section section = (e.channel == Channel.VITALS) ? vitals : telemetry;
        section.list.remove(e.chip);

        entries.remove(e);

        section.list.revalidate();
        section.list.repaint();
    }

    static Color colorFor(Status s) {
        return switch (s) {
            case OK -> new Color(76, 175, 80);
            case WARNING -> new Color(255, 235, 59);
            case CRITICAL -> new Color(244, 67, 54);
        };
    }

    //ease of life shortcuts for posting
    public void TelemetryUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            post(status,
                NotificationPanel.Channel.TELEMETRY, msg);
        });
    }
    public void VitalsUpdate(String msg, NotificationPanel.Status status) {
        SwingUtilities.invokeLater(() -> {
            post(status,
                NotificationPanel.Channel.VITALS, msg);
        });
    }

}
