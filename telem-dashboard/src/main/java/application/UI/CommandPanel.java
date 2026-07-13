package application.UI;
import javax.swing.*;

import presentation.CommandRecords;
import presentation.CommandRecords.Command;
import presentation.CommandRecords.EnumEntry;

import java.awt.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

//handles overall connection between UI <-> application <-> serial bridge
public class CommandPanel extends JPanel {

    private final NotificationPanel notifications;
    private final Consumer<byte[]> sendCommandCallback;

    private final JComboBox<CommandRecords.Command> commandSelector;
    private final JPanel fieldsPanel;
    private final JButton sendButton;

    private final Map<CommandRecords.CommandField, JComponent> inputFieldMap = new HashMap<>();
    private JTextField customPayloadField;

    public CommandPanel(NotificationPanel notifications, Consumer<byte[]> sendCommandCallback) {
        super(new BorderLayout(6, 6));
        this.notifications = notifications;
        this.sendCommandCallback = sendCommandCallback;

        setBorder(BorderFactory.createTitledBorder("Command Center"));

        // Top panel for command selection
        JPanel topPanel = new JPanel(new BorderLayout());
        topPanel.add(new JLabel("Command: "), BorderLayout.WEST);
        commandSelector = new JComboBox<>(CommandRecords.COMMANDS.toArray(new CommandRecords.Command[0]));
        topPanel.add(commandSelector, BorderLayout.CENTER);

        // Center panel for dynamic fields
        fieldsPanel = new JPanel(new GridBagLayout());
        JScrollPane scrollPane = new JScrollPane(fieldsPanel);
        scrollPane.setBorder(BorderFactory.createEmptyBorder());

        // Bottom panel for the send button
        sendButton = new JButton("Send Command");

        add(topPanel, BorderLayout.NORTH);
        add(scrollPane, BorderLayout.CENTER);
        add(sendButton, BorderLayout.SOUTH);

        commandSelector.addActionListener(e -> renderFieldsForSelectedCommand());
        sendButton.addActionListener(e -> packAndSendCommand());

        // Initial render
        renderFieldsForSelectedCommand();
    }

    private void renderFieldsForSelectedCommand() {
        fieldsPanel.removeAll();
        inputFieldMap.clear();
        customPayloadField = null;

        CommandRecords.Command selected = (CommandRecords.Command) commandSelector.getSelectedItem();
        if (selected == null) {
            fieldsPanel.revalidate();
            fieldsPanel.repaint();
            return;
        }

        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(2, 4, 2, 4);
        gbc.anchor = GridBagConstraints.WEST;

        int gridY = 0;

        for (CommandRecords.CommandField field : selected.fields()) {
            gbc.gridx = 0;
            gbc.gridy = gridY;
            gbc.fill = GridBagConstraints.NONE;
            gbc.weightx = 0;
            fieldsPanel.add(new JLabel(field.name() + ":"), gbc);

            gbc.gridx = 1;
            gbc.fill = GridBagConstraints.HORIZONTAL;
            gbc.weightx = 1;

            JComponent inputComponent;
            if (field.enumName() != null) {
                List<CommandRecords.EnumEntry> entries = CommandRecords.ENUMS.get(field.enumName());
                if (entries == null) {
                    notifications.post(NotificationPanel.Status.CRITICAL, NotificationPanel.Channel.TELEMETRY,
                            "Command field '" + field.name() + "' references unknown enum '" + field.enumName() + "'. Falling back to text input.");
                    inputComponent = new JTextField(10); // Fallback to text field
                } else {
                    inputComponent = new JComboBox<>(entries.toArray(new CommandRecords.EnumEntry[0]));
                }
            } else {
                inputComponent = new JTextField(10);
            }
            fieldsPanel.add(inputComponent, gbc);
            inputFieldMap.put(field, inputComponent);
            gridY++;
        }

        if (selected.isCustom()) {
            gbc.gridx = 0;
            gbc.gridy = gridY;
            gbc.fill = GridBagConstraints.NONE;
            gbc.weightx = 0;
            fieldsPanel.add(new JLabel("Custom Payload (Hex):"), gbc);

            gbc.gridx = 1;
            gbc.fill = GridBagConstraints.HORIZONTAL;
            gbc.weightx = 1;
            customPayloadField = new JTextField();
            fieldsPanel.add(customPayloadField, gbc);
        }

        fieldsPanel.revalidate();
        fieldsPanel.repaint();
    }

    private void packAndSendCommand() {
        CommandRecords.Command selected = (CommandRecords.Command) commandSelector.getSelectedItem();
        if (selected == null) return;

        BitPacker packer = new BitPacker();

        // 1. Pack the command mask
        packer.pack(selected.mask(), selected.maskBits());

        // 2. Pack all fixed fields
        for (CommandRecords.CommandField field : selected.fields()) {
            JComponent input = inputFieldMap.get(field);
            long value;

            try {
                if (input instanceof JComboBox) {
                    value = ((CommandRecords.EnumEntry) ((JComboBox<?>) input).getSelectedItem()).value();
                } else {
                    String text = ((JTextField) input).getText();
                    value = parseNumber(text);
                }

                // Validate
                if (value < field.min() || value > field.max()) {
                    throw new NumberFormatException("Value " + value + " out of range [" + field.min() + ", " + field.max() + "]");
                }

                packer.pack(value, field.bits());

            } catch (Exception ex) {
                notifications.post(NotificationPanel.Status.CRITICAL, NotificationPanel.Channel.TELEMETRY,
                        "Invalid input for '" + field.name() + "': " + ex.getMessage());
                return;
            }
        }

        // 3. Pack custom payload if it exists
        if (customPayloadField != null) {
            String hexPayload = customPayloadField.getText();
            if (hexPayload != null && !hexPayload.isBlank()) {
                try {
                    byte[] customBytes = hexStringToByteArray(hexPayload);
                    packer.packBytes(customBytes);
                } catch (Exception ex) {
                    notifications.post(NotificationPanel.Status.CRITICAL, NotificationPanel.Channel.TELEMETRY,
                            "Invalid hex string for custom payload: " + ex.getMessage());
                    return;
                }
            }
        }

        // 4. Send the final byte array
        sendCommandCallback.accept(packer.toByteArray());
        notifications.post(NotificationPanel.Status.OK, NotificationPanel.Channel.TELEMETRY, "Command '" + selected.name() + "' sent.");
    }

    private long parseNumber(String s) throws NumberFormatException {
        if (s == null || s.isBlank()) return 0;
        s = s.trim();
        return Long.decode(s); // Handles "0x", "0b", and decimal
    }

    private byte[] hexStringToByteArray(String s) {
        s = s.replaceAll("\\s", "");
        if (s.length() % 2 != 0) {
            throw new IllegalArgumentException("Hex string must have an even number of characters.");
        }
        int len = s.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                                 + Character.digit(s.charAt(i + 1), 16));
        }
        return data;
    }

    /** Helper class for packing bit-level data into a byte array. */
    private static class BitPacker {
        private final ByteBuffer buffer = ByteBuffer.allocate(255).order(ByteOrder.LITTLE_ENDIAN);
        private long bitBuffer = 0;
        private int bitCount = 0;

        public void pack(long value, int numBits) {
            if (numBits <= 0 || numBits > 64) return;
            bitBuffer |= (value & ((1L << numBits) - 1)) << bitCount;
            bitCount += numBits;
            while (bitCount >= 8) {
                buffer.put((byte) (bitBuffer & 0xFF));
                bitBuffer >>>= 8;
                bitCount -= 8;
            }
        }

        public void packBytes(byte[] bytes) {
            alignToByte();
            buffer.put(bytes);
        }

        private void alignToByte() {
            if (bitCount > 0) {
                buffer.put((byte) (bitBuffer & 0xFF));
                bitBuffer = 0;
                bitCount = 0;
            }
        }

        public byte[] toByteArray() {
            alignToByte();
            byte[] result = new byte[buffer.position()];
            buffer.flip();
            buffer.get(result);
            return result;
        }
    }
}