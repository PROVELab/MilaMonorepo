package util;

import com.fazecast.jSerialComm.SerialPort;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.function.Consumer;
import java.util.function.Function;

public final class SerialBridge implements AutoCloseable {
    private final String portName;
    private final int baud;
    private final Function<byte[], Integer> onMessageRecv;
    private final Consumer<Boolean> onStatusChange;

    private SerialPort port;
    private OutputStream out;
    private Thread readerThread;
    private volatile boolean running = false;

    public SerialBridge(String portName, int baud,
                        Function<byte[], Integer> onMessageRecv,
                        Consumer<Boolean> onStatusChange) {
        this.portName = portName;
        this.baud = baud;
        this.onMessageRecv = onMessageRecv;
        this.onStatusChange = onStatusChange;
    }

    public synchronized boolean connect() {
        if (running) {
            System.out.println("SerialBridge is already running or connecting.");
            return true;
        }
        try {
            this.port = SerialPort.getCommPort(portName);
            port.setBaudRate(baud);
            port.setNumDataBits(8);
            port.setParity(SerialPort.NO_PARITY);
            port.setNumStopBits(SerialPort.ONE_STOP_BIT);
            port.setComPortTimeouts(SerialPort.TIMEOUT_READ_BLOCKING, 1000, 0);
            port.setFlowControl(SerialPort.FLOW_CONTROL_DISABLED);
            
            if (!port.openPort()) {
                throw new IOException("Failed to open port " + portName);
            }
            port.setDTR();
            port.setRTS();

            try { Thread.sleep(2000); } catch (InterruptedException ignored) {}
            this.out = port.getOutputStream();
            startReader();
            onStatusChange.accept(true);
            return true;
        } catch (Exception e) {
            System.err.println("SerialBridge connection failed: " + e.getMessage());
            close(); // Ensure everything is cleaned up
            return false;
        }
    }

    private void startReader() {
        if (running) return;
        running = true;
        readerThread = new Thread(() -> {
            System.out.println("running recv thread");
            try (InputStream in = port.getInputStream()) {
                System.out.println("init success");
                while (running) {
                    try {
                        receiveFrames(in);
                    } catch (IOException e) {
                        if (running) System.err.println("[SerialBridge read error] " + e.getMessage());
                        close(); // Connection lost, trigger close
                        break; // Exit thread loop
                    }
                }
                System.out.println("reader thread exiting");
            } catch (IOException e) {
                if (running) {
                    System.err.println("[SerialBridge stream error] " + e.getMessage());
                    close();
                }
            }
        }, "serial-reader");
        readerThread.setDaemon(true);
        readerThread.start();
    }

    public boolean isConnected() {
        return running;
    }

    // Circular buffer state to recv messages
    private static final int RX_CAP = 256;
    private final byte[] rx = new byte[RX_CAP];
    private int head = 0;   // start of valid data
    private int len  = 0;   // bytes of valid data

    private void receiveFrames(InputStream in) throws IOException {
        final byte SOF = (byte) 0xFF;

        // Read available bytes into ring buffer
        int avail = port.bytesAvailable();
        if (avail > 0 && space() > 0) {
            int tail = (head + len) % RX_CAP;
            int toRead = Math.min(avail, space());
            int c1 = Math.min(toRead, RX_CAP - tail);
            int r1 = in.read(rx, tail, c1);
            if (r1 > 0) {
                len += r1;
                toRead -= r1;
            }
            if (toRead > 0) {
                int r2 = in.read(rx, 0, Math.min(toRead, RX_CAP - len));
                if (r2 > 0) {
                    len += r2;
                }
            }
        }

        // Parse log lines and binary frames from the buffer
        while (len > 0) {
            // First, try to process any complete log lines (I/W/E/D messages)
            int newlineOff = indexOf((byte) '\n', 0, len);
            if (newlineOff >= 0) {
                byte firstByte = (byte) get(0);
                // Check for standard ESP-IDF log prefixes
                if (firstByte == 'I' || firstByte == 'W' || firstByte == 'E' || firstByte == 'D') {
                    byte[] lineBytes = new byte[newlineOff];
                    copyOut(lineBytes, 0, 0, newlineOff);
                    System.out.println("[ESP32 Log] " + new String(lineBytes).trim());
                    drop(newlineOff + 1); // Consume the line from the buffer
                    continue; // Check for more lines
                }
            }

            // If we're here, the buffer doesn't start with a complete I/W/E/D log line.
            // Now, look for a binary frame, which starts with SOF.
            int sofOff = indexOf(SOF, 0, len);

            if (sofOff < 0) {
                // No SOF found. If the buffer is full of junk, discard it to prevent getting stuck.
                if (len == RX_CAP) {
                    System.out.println("[SerialBridge] Buffer full with no SOF, discarding.");
                    drop(len);
                }
                break; // Wait for more data
            }

            // We found a SOF. Handle any data before it.
            if (sofOff > 0) {
                // This could be our "B (...)" marker or other junk.
                byte[] prefixBytes = new byte[sofOff];
                copyOut(prefixBytes, 0, 0, sofOff);
                String prefix = new String(prefixBytes).trim();
                if (prefix.startsWith("B")) {
                    System.out.println("[ESP32 Log] " + prefix); // Print our binary data marker
                } else if (!prefix.isEmpty()) {
                    System.out.println("[SerialBridge] Discarding pre-SOF junk: " + prefix);
                }
                drop(sofOff);
                continue; // Re-run the loop, SOF will now be at the start of the buffer.
            }

            // SOF is at the head. Now, parse the variable-length C++ frame format.
            // C++ sends: [SOF(1)][LEN(2)][PAYLOAD(N)][CSUM(2)]
            final int MIN_FRAME_LEN = 1 + 2 + 0 + 2; // SOF + len + (empty payload) + checksum
            if (len < MIN_FRAME_LEN) {
                break; // Not enough data for even the smallest frame.
            }

            // Get payload length (bytes 1 and 2 after SOF, Little Endian)
            int payloadLen = (get(1) & 0xFF) | ((get(2) & 0xFF) << 8);

            // Sanity check on length to prevent huge buffer allocation or reading past buffer
            if (payloadLen < 0 || payloadLen > RX_CAP * 2) { // Allow slightly larger than buffer for robustness, but not insane
                System.out.println("[SerialBridge warning] Insane payload length: " + payloadLen + ". Discarding SOF.");
                drop(1); // Drop bad SOF and retry
                continue;
            }

            int frameTotalLen = 1 + 2 + payloadLen + 2; // SOF + len_bytes + payload + csum_bytes
            if (len < frameTotalLen) {
                break; // Not enough data for the full frame yet.
            }

            // We have a full frame. Extract payload.
            byte[] payload = new byte[payloadLen];
            copyOut(payload, 0, 1 + 2, payloadLen); // copy from after SOF and length bytes

            // Extract checksum from the end of the frame
            int csumOffset = 1 + 2 + payloadLen;
            int recvChk = (get(csumOffset) & 0xFF) | ((get(csumOffset + 1) & 0xFF) << 8);

            // Calculate checksum on the extracted payload
            int calcChk = inetChecksum16(payload, 0, payloadLen);

            if (calcChk == recvChk) {
                // The payload from LoRa can contain multiple smaller data packets.
                // We loop, calling the parser on the remaining payload until it's all consumed.
                int totalBytesConsumed = 0;
                while (totalBytesConsumed < payload.length) {
                    byte[] remainingPayload = new byte[payload.length - totalBytesConsumed];
                    System.arraycopy(payload, totalBytesConsumed, remainingPayload, 0, remainingPayload.length);

                    // The callback must return the number of bytes it consumed from the start of the slice.
                    int consumed = onMessageRecv.apply(remainingPayload);

                    if (consumed > 0) {
                        totalBytesConsumed += consumed;
                    } else {
                        // Parser consumed 0 bytes, indicating an error or end of parsable data.
                        if (remainingPayload.length > 0) {
                            System.err.println("[SerialBridge] Parser stalled. Discarding " + remainingPayload.length + " remaining bytes of payload.");
                        }
                        break; // Exit loop to avoid getting stuck.
                    }
                }
            } else {
                System.out.println(String.format(
                        "[SerialBridge warning] Checksum failed for C++ frame: got=0x%04X expected=0x%04X. Discarding SOF and retrying.",
                        recvChk, calcChk));
                drop(1); // Drop just the bad SOF and try to re-sync
                continue;
            }
            drop(frameTotalLen); // Drop the successfully processed frame
        }
    }

    /* ===== helpers for circular buffer used by recvFrames ===== */
    private int space() { return RX_CAP - len; }
    private int get(int off) { return rx[(head + off) % RX_CAP] & 0xFF; }
    private void copyOut(byte[] dst, int dstOff, int srcOff, int n) {
        int start = (head + srcOff) % RX_CAP;
        int c1 = Math.min(n, RX_CAP - start);
        System.arraycopy(rx, start, dst, dstOff, c1);
        if (n > c1) System.arraycopy(rx, 0, dst, dstOff + c1, n - c1);
    }
    private void drop(int n) { head = (head + n) % RX_CAP; len -= n; if (len < 0) { head = 0; len = 0; } }
    private int indexOf(byte b, int off, int count) {
        for (int i = 0; i < count; i++) if ((byte)get(off + i) == b) return off + i;
        return -1;
    }
    /* =========== */

    private int inetChecksum16(byte[] data, int off, int len) {
        long sum = 0;                       // 32-bit accumulator
        int i = off;
        int end = off + len;

        // Sum 16-bit words (LE pairing)
        while (i + 1 < end) {
            int word = (data[i] & 0xFF) | ((data[i + 1] & 0xFF) << 8); // low,high
            sum += word;
            i += 2;
        }

        // Odd trailing byte goes into LOW byte
        if (i < end) {
            sum += (data[i] & 0xFF);
        }

        // Fold to 16 bits with end-around carry, then one's complement
        sum = (sum & 0xFFFF) + (sum >> 16);
        sum = (sum & 0xFFFF) + (sum >> 16);
        return (int)(~sum) & 0xFFFF;
    }

    // Send a message with variable length payload.
    // Frame: [SOF=0xFF][LEN 2b LE][DATA][CHK16 2b LE]
    public void sendMessage(byte[] data) throws IOException {
        if (data == null) {
            System.out.println("ignoring null msg ");
            return;
        }

        int payloadLen = data.length;
        int chk16 = inetChecksum16(data, 0, payloadLen) & 0xFFFF;

        byte[] frame = new byte[1 + 2 + payloadLen + 2];
        ByteBuffer bb = ByteBuffer.wrap(frame).order(ByteOrder.LITTLE_ENDIAN);
        bb.put((byte) 0xFF);
        bb.putShort((short) payloadLen);
        bb.put(data);
        bb.putShort((short) chk16);

        synchronized (out) {
            out.write(frame);
            out.flush();
        }
    }
    @Override
    public void close() {
        running = false;
        if (port != null && port.isOpen()) port.closePort();  // breaks blocking read
        if (readerThread != null) {
            readerThread.interrupt();
            try { readerThread.join(500); } catch (InterruptedException ignored) {}
        }
        try { if (out != null) out.close(); } catch (IOException ignored) {}
    }
}
