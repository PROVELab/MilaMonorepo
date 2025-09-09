import javax.swing.*;
import javax.swing.border.TitledBorder;
import java.awt.*;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

public class NotificationPanel extends JPanel {

    // Public API
    public enum Channel { VITALS, TELEMETRY }
    public enum Status  { OK, WARNING, CRITICAL }

    public static final class NotificationHandle {
        private final NotificationPanel owner;
        private final Channel channel;
        private final Status status;
        private UUID id;

        private NotificationHandle(NotificationPanel owner, Channel ch, Status st, UUID id) {
            this.owner = owner; this.channel = ch; this.status = st; this.id = id;
        }

        public void updateText(String newText) {
            SwingUtilities.invokeLater(() -> id = owner.updateOrRecreate(channel, status, id, newText));
        }

        public void close() { SwingUtilities.invokeLater(() -> owner.removeById(id)); }
    }

    public NotificationHandle post(Status status, Channel channel, String text) {
        UUID id = UUID.randomUUID();
        SwingUtilities.invokeLater(() -> createChip(id, status, channel, text));
        return new NotificationHandle(this, channel, status, id);
    }

    // Layout constants (original “two-line-ish” behavior)
    private static final int CHIP_HEIGHT = 30; //heigh of one line
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

    // Entry (state)
    private static final class Entry {
        final UUID id;
        final Channel channel;
        final Status status;
        final JPanel chip;     // BorderLayout
        final JTextArea text;  // wrapped
        final JButton expandBtn; // ▸/▾
        final JButton closeBtn;  // ×
        final JPanel square;   // status color
        boolean expanded = false;

        Entry(UUID id, Channel ch, Status st, JPanel chip, JTextArea text,
              JButton expandBtn, JButton closeBtn, JPanel square) {
            this.id = id; this.channel = ch; this.status = st;
            this.chip = chip; this.text = text; this.expandBtn = expandBtn;
            this.closeBtn = closeBtn; this.square = square;
        }
    }

    // Two halves
    private final Section vitals    = new Section("Vitals Notifications");
    private final Section telemetry = new Section("Telemetry Notifications");
    private final Map<UUID, Entry> entries = new HashMap<>();

    public NotificationPanel() {
        super(new GridLayout(2, 1));
        setPreferredSize(new Dimension(260, 0));
        add(vitals.root);
        add(telemetry.root);
    }

    private UUID updateOrRecreate(Channel channel, Status status, UUID currentId, String newText) {
        Entry e = entries.get(currentId);
        if (e != null) {
            e.text.setText(newText);
            updateOverflowVisibility(e, (e.channel == Channel.VITALS) ? vitals : telemetry);
            if (e.expanded) applyExpandedSize(e, (e.channel == Channel.VITALS) ? vitals : telemetry);
            e.chip.revalidate(); e.chip.repaint();
            return currentId;
        }
        UUID newId = UUID.randomUUID();
        createChip(newId, status, channel, newText);
        return newId;
    }

    // ==================== ORIGINAL “two-line-ish” create ====================

    private void createChip(UUID id, Status status, Channel channel, String msg) {
        JPanel chip = new JPanel(new BorderLayout(GAP, 0));
        chip.setBorder(BorderFactory.createCompoundBorder(
                BorderFactory.createEmptyBorder(4, 8, 4, 8),
                BorderFactory.createLineBorder(new Color(0,0,0,40))
        ));
        chip.setOpaque(false);
        chip.setAlignmentX(Component.LEFT_ALIGNMENT);

        // WEST: colored square
        JPanel square = new JPanel();
        square.setPreferredSize(new Dimension(14, 14));
        square.setMaximumSize(new Dimension(14, 14));
        square.setBackground(colorFor(status));
        square.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY, 1));

        JPanel squareWrap = new JPanel(new GridBagLayout());
        squareWrap.setOpaque(false);
        squareWrap.add(square);
        chip.add(squareWrap, BorderLayout.WEST);

        // CENTER: JTextArea (wrap)
        JTextArea text = new JTextArea(msg);
        text.setLineWrap(true);
        text.setWrapStyleWord(true);
        text.setEditable(false);
        text.setOpaque(false);
        text.setBorder(BorderFactory.createEmptyBorder());
        chip.add(text, BorderLayout.CENTER);

        // EAST: expand/close buttons
        JButton expand = new JButton("▸"); // visible in this version
        stylizeMiniButton(expand);
        expand.setToolTipText("Expand/collapse");

        JButton close = new JButton("×");
        stylizeMiniButton(close);
        close.setForeground(Color.RED.darker());
        close.setToolTipText("Dismiss");

        JPanel right = new JPanel();
        right.setOpaque(false);
        right.setLayout(new BoxLayout(right, BoxLayout.X_AXIS));
        right.add(expand);
        right.add(Box.createHorizontalStrut(4));
        right.add(close);
        chip.add(right, BorderLayout.EAST);

        Entry entry = new Entry(id, channel, status, chip, text, expand, close, square);
        entries.put(id, entry);

        Section section = (channel == Channel.VITALS) ? vitals : telemetry;

        // insert newest at top
        section.list.add(chip, 0);

        // start collapsed
        applyCollapsedSize(entry);

        // show expand by default; hide later if we detect no overflow
        expand.setVisible(true);

        // after first layout: do a simple overflow check
        SwingUtilities.invokeLater(() -> {
            updateOverflowVisibility(entry, section);
            if (!entry.expanded) applyCollapsedSize(entry);
        });

        // react to chip width changes (simple logic)
        chip.addComponentListener(new java.awt.event.ComponentAdapter() {
            @Override public void componentResized(java.awt.event.ComponentEvent e) {
                updateOverflowVisibility(entry, section);
                if (entry.expanded) applyExpandedSize(entry, section);
            }
        });

        // toggle expand/collapse (this is the version that tended to show ~2 lines)
        expand.addActionListener(ae -> {
            entry.expanded = !entry.expanded;
            expand.setText(entry.expanded ? "▾" : "▸");
            if (entry.expanded) applyExpandedSize(entry, section);
            else applyCollapsedSize(entry);
            section.list.revalidate();
            section.list.repaint();
        });

        // close
        close.addActionListener(ae -> removeById(id));

        section.list.revalidate();
        section.list.repaint();
    }

    private void stylizeMiniButton(JButton b) {
        b.setMargin(new Insets(1, 6, 1, 6));
        b.setFocusPainted(false);
        b.setBorder(BorderFactory.createEmptyBorder());
        b.setContentAreaFilled(false);
    }

    // Collapsed = cap chip height to a single line area
    private void applyCollapsedSize(Entry e) {
        int oneLine = e.text.getFontMetrics(e.text.getFont()).getHeight() + 2;

        // Cap text to 1 line (this was part of the earlier behavior)
        // e.text.setPreferredSize(null);
        // e.text.setMaximumSize(new Dimension(Integer.MAX_VALUE, oneLine));

        // Cap chip
        e.chip.setMinimumSize(new Dimension(0, CHIP_HEIGHT));
        e.chip.setPreferredSize(new Dimension(0, CHIP_HEIGHT));
        e.chip.setMaximumSize(new Dimension(Integer.MAX_VALUE, CHIP_HEIGHT));
    }

    // Expanded = remove the 1-line cap but measure height using a “best guess” width (the earlier approach)
    private void applyExpandedSize(Entry e, Section section) {
        // Earlier version relied on current text width (often stale) → tends to give ~2 lines
        int w = Math.max(1, e.text.getWidth());

        Dimension pref = e.text.getPreferredSize();
        int target = Math.max(CHIP_HEIGHT, pref.height + CHIP_VPAD * 2);

        e.chip.setMinimumSize(new Dimension(0, target));
        e.chip.setPreferredSize(new Dimension(0, target));
        e.chip.setMaximumSize(new Dimension(Integer.MAX_VALUE, target));

        e.text.revalidate();
        e.chip.revalidate();
    }

    // Simple overflow detection from the earlier version
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

    private void removeById(UUID id) {
        Entry e = entries.remove(id);
        if (e == null) return;
        Section section = (e.channel == Channel.VITALS) ? vitals : telemetry;
        section.list.remove(e.chip);
        section.list.revalidate();
        section.list.repaint();
    }

    private static Color colorFor(Status s) {
        return switch (s) {
            case OK -> new Color(76, 175, 80);
            case WARNING -> new Color(255, 235, 59);
            case CRITICAL -> new Color(244, 67, 54);
        };
    }

    // Optional
    public Set<UUID> liveNotificationIds() { return entries.keySet(); }
}
