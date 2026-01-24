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
    // presenceMap : key -> isPresent (true if that chip is already posted). only enforced if oneBtn is pressed
    private final Map<DedupKey, Boolean> presenceMap = new HashMap<>();

    //for sending commands:
    private Consumer<String> onCommandSubmit = null;
    public void setOnCommandSubmit(Consumer<String> handler) {
        this.onCommandSubmit = handler;
    }
    public void setCommandPrompt(String text) {
        commandBar.setPrompt(text);
    }

    // Used as a handler for each notification 
    public static final class Entry {
        final NotificationPanel owner;

        final Channel channel;
        Status status;
        final JPanel chip;
        final JTextArea text;
        final JButton expandBtn;    //arrow to expand text box
        final JButton oneBtn;      // 1 Btn to allow for limiting identical message to one instance of themself.
        final JButton closeBtn;     //X to close notifications
        final JPanel square;        //square the message is stored on
        final JLabel ageLabel;     // Indicates age of message in seconds
        boolean expanded = false;
        boolean posted = true;     // track whether still visible
        boolean oneActive = false; // whether this chip’s “1” is active (underlined)
        int ageSeconds = 0; 

        Entry(NotificationPanel owner, Channel ch, Status st, JPanel chip, JTextArea text,
          JButton expandBtn, JButton oneBtn, JButton closeBtn, JPanel square, JLabel ageLabel) {
        this.owner = owner;
        this.channel = ch; this.status = st;
        this.chip = chip; this.text = text;
        this.expandBtn = expandBtn; this.oneBtn = oneBtn; this.closeBtn = closeBtn; this.square = square;
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
            p.resetAge(this);
            Section section = (channel == Channel.VITALS) ? p.vitals : p.telemetry;
            p.updateOverflowVisibility(this, section);
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
            owner.resetAge(this);
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

        // presence bookkeeping
        DedupKey key = keyOf(e);
        if (e.oneActive) {
            // if this chip’s “1” is active, enforce it
            setOneUnderline(e.oneBtn, true);
            presenceMap.put(key, Boolean.TRUE);
            closeOtherMatches(e, key);
        } else if (presenceMap.containsKey(key)) {
            // filter exists for this key → mark presence = true
            presenceMap.put(key, Boolean.TRUE);
        }

        // size/overflow refresh
        updateOverflowVisibility(e, section);
        if (!e.expanded) applyCollapsedSize(e);

        section.list.revalidate();
        section.list.repaint();
    }

    //reset a chip's age to 0s
    private void resetAge(Entry e) {
        e.ageSeconds = 0;
        e.ageLabel.setText("0s");
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
    // Command bar for sending from telem
    private final CommandBar commandBar = new CommandBar();

    private final javax.swing.Timer ageTicker;

    //The panel holding all the notifications
    public NotificationPanel() {
        super(new BorderLayout(0, 6));
        setPreferredSize(new Dimension(260, 0));

        JPanel stacked = new JPanel();
        stacked.setLayout(new GridLayout(2, 1, 0, 6));
        stacked.add(vitals.root);
        stacked.add(telemetry.root);

        add(stacked, BorderLayout.CENTER);
        add(commandBar, BorderLayout.SOUTH);

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

    //create a new chip (notification) to put on panel
    private Entry createChip(Status status, Channel channel, String msg) {
        DedupKey key = new DedupKey(channel, status, msg);
        if (presenceMap.containsKey(key)) { //if the chip is already present, dont remake, just update chip timer
            boolean isPresent = Boolean.TRUE.equals(presenceMap.get(key));
            if (isPresent) {
                for (Entry e : entries) {   //find the chip, and update timer
                    if (e.posted && key.equals(keyOf(e))){
                        resetAge(e);
                        return e;   
                    } 
                }
                // chip was closed, lets add it back
                presenceMap.put(key, Boolean.FALSE);    //indicate not yet posted, is set to true later
            }
        }

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

        JButton one = new JButton("1");                 
        stylizeMiniButton(one);
        one.setToolTipText("Allow only one of this message");

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
        right.add(one);                                
        right.add(Box.createHorizontalStrut(4));
        right.add(close);
        chip.add(right, BorderLayout.EAST);
        //

        // create the entry
        Entry entry = new Entry(this, channel, status, chip, text, expand, one, close, square, age);
        resetAge(entry);

        Section section = (channel == Channel.VITALS) ? vitals : telemetry;
        section.list.add(chip, 0);

        applyCollapsedSize(entry);
        expand.setVisible(true);

        // update the map tracking what chips are posted to indicate this is posted
        if (presenceMap.containsKey(key) && !Boolean.TRUE.equals(presenceMap.get(key))) {
            setOneUnderline(one, true);
            entry.oneActive = true;
            presenceMap.put(key, Boolean.TRUE);
            // ensure no strays
            closeOtherMatches(entry, key);
        }

        SwingUtilities.invokeLater(() -> {
            updateOverflowVisibility(entry, section);
            if (!entry.expanded) applyCollapsedSize(entry);
        });

        //add listeners for the buttons
        chip.addComponentListener(new java.awt.event.ComponentAdapter() {
            @Override public void componentResized(java.awt.event.ComponentEvent e) {
                updateOverflowVisibility(entry, section);
                if (entry.expanded) applyExpandedSize(entry, section);
            }
        });

        // expansion button
        expand.addActionListener(ae -> {
            entry.expanded = !entry.expanded;
            expand.setText(entry.expanded ? "▾" : "▸");
            if (entry.expanded) applyExpandedSize(entry, section);
            else applyCollapsedSize(entry);
            section.list.revalidate();
            section.list.repaint();
        });

        // 1 button
        one.addActionListener(ae -> {
            DedupKey k = keyOf(entry);
            if (!entry.oneActive) {
                // Activate filter for this key; this chip is the one allowed.
                setOneUnderline(one, true);
                entry.oneActive = true;
                presenceMap.put(k, Boolean.TRUE);
                closeOtherMatches(entry, k);
            } else {
                // Deactivate filter for this key; allow multiples again.
                setOneUnderline(one, false);
                entry.oneActive = false;
                presenceMap.remove(k);
            }
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

    private void setOneUnderline(JButton b, boolean on) { // indicate the 1 button is active (at most one of this chip posted)
        b.setText(on ? "<html><u>1</u></html>" : "1");
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

    private void updateOverflowVisibility(Entry e, Section section) {
        int w = Math.max(1, e.text.getWidth());
        e.text.setPreferredSize(null);
        e.text.setMaximumSize(null);
        e.text.setSize(new Dimension(w, Short.MAX_VALUE));

        Dimension pref = e.text.getPreferredSize();
        int oneLine = e.text.getFontMetrics(e.text.getFont()).getHeight() + 2;
        boolean overflow = pref.height > oneLine;

        e.expandBtn.setVisible(overflow);

        if (!overflow && e.expanded) {
            e.expanded = false;
            e.expandBtn.setText("▸");
            applyCollapsedSize(e);
        }
    }

    private void unPostEntry(Entry e) {
        e.posted = false;
        Section section = (e.channel == Channel.VITALS) ? vitals : telemetry;
        section.list.remove(e.chip);

        // indicate the chip is no longer posted in duplicate lookup
        DedupKey key = keyOf(e);
        if (presenceMap.containsKey(key)) {
            // Is there another posted entry with the same key still visible?
            boolean anyPosted = false;
            for (Entry other : entries) {
                if (other != e && other.posted && key.equals(keyOf(other))) {
                    anyPosted = true; break;
                }
            }
            presenceMap.put(key, anyPosted);
        }

        entries.remove(e);

        section.list.revalidate();
        section.list.repaint();
    }

    // Close all other posted chips that match this key (used when enabling “1”)
    private void closeOtherMatches(Entry keep, DedupKey key) {
        for (Entry other : new ArrayList<>(entries)) { // copy to avoid concurrent modification
            if (other != keep && other.posted && key.equals(keyOf(other))) {
                other.unPostEntry(this);
            }
        }
    }

    // Build key for an entry (exact message, channel, status)
    private DedupKey keyOf(Entry e) {
        return new DedupKey(e.channel, e.status, e.text.getText());
    }

    // Key class for the presenceMap
    private static final class DedupKey {
        final Channel ch;
        final Status st;
        final String msg;
        DedupKey(Channel ch, Status st, String msg) {
            this.ch = ch; this.st = st; this.msg = msg;
        }
        @Override public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof DedupKey k)) return false;
            return ch == k.ch && st == k.st && Objects.equals(msg, k.msg);
        }
        @Override public int hashCode() {
            return Objects.hash(ch, st, msg);
        }
    }

    static Color colorFor(Status s) {
        return switch (s) {
            case OK -> new Color(76, 175, 80);
            case WARNING -> new Color(255, 235, 59);
            case CRITICAL -> new Color(244, 67, 54);
        };
    }

    // the CommandBar
    private final class CommandBar extends JPanel {
        private final JLabel prompt = new JLabel("Command:");
        private final JTextField field = new JTextField();

        CommandBar() {
            super(new BorderLayout(6, 0));
            setBorder(BorderFactory.createCompoundBorder(
                    BorderFactory.createTitledBorder(
                            BorderFactory.createLineBorder(Color.GRAY),
                            "Command"),
                    BorderFactory.createEmptyBorder(6,6,6,6)
            ));
            field.setColumns(12);
            add(prompt, BorderLayout.WEST);
            add(field, BorderLayout.CENTER);

            field.addActionListener(e -> {
                String s = field.getText().trim();
                if (s.isEmpty()) return;
                Consumer<String> cb = onCommandSubmit;
                if (cb != null) cb.accept(s);
                field.setText("");
            });

            setMaximumSize(new Dimension(Integer.MAX_VALUE, field.getPreferredSize().height + 30));
        }

        void setPrompt(String text) { prompt.setText(text); }
    }
}
