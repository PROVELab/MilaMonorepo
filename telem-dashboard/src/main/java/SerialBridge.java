package util;

import com.fazecast.jSerialComm.SerialPort;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.function.Consumer;

public final class SerialBridge implements AutoCloseable {
    private final String portName;
    private final int baud;
    private final Consumer<byte[]> onMessageRecv;
    private final Consumer<Boolean> onStatusChange;
    private final String logFileName;

    private SerialPort port;
    private OutputStream out;
    private FileOutputStream logFileStream;
    private Thread readerThread;
    private volatile boolean running = false;

    public SerialBridge(String portName, int baud,
                        Consumer<byte[]> onMessageRecv,
                        Consumer<Boolean> onStatusChange,
                        String logFileName) {
        this.portName = portName;
        this.baud = baud;
        this.onMessageRecv = onMessageRecv;
        this.onStatusChange = onStatusChange;
        this.logFileName = logFileName;
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
            //needs to be semi-blocking (not blocking) so we dont stall a full second before parsing every time
            port.setComPortTimeouts(SerialPort.TIMEOUT_READ_SEMI_BLOCKING, 1000, 0);
            port.setFlowControl(SerialPort.FLOW_CONTROL_DISABLED);
            
            if (!port.openPort()) {
                throw new IOException("Failed to open port " + portName);
            }
            port.setDTR();
            port.setRTS();

            try { Thread.sleep(2000); } catch (InterruptedException ignored) {}
            this.out = port.getOutputStream();
            this.logFileStream = new FileOutputStream(this.logFileName);
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
                        // A read timeout in jSerialComm throws an IOException. This is expected if the
                        // ESP32 is silent. We should not treat it as a fatal error, but just loop again.
                        if (e.getMessage() != null && e.getMessage().contains("timed out")) {
                            // This is a normal timeout. Continue waiting for data.
                            continue;
                        }
                        // For any other I/O error, we assume the connection is lost.
                        if (running) System.err.println("[SerialBridge fatal read error] " + e.getMessage());
                        close(); // Close the connection.
                        break;   // Exit the reader thread loop.
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

    // Circular buffer state to receive mixed binary frames and ESP32 log text.
    // A single valid binary frame can be 276 bytes:
    //   SOF(1) + LEN(2) + metadata+LoRa payload(271 max) + checksum(2)
    // The buffer must be comfortably larger than that because log text can
    // arrive adjacent to a frame before we've drained the serial port.
    private static final int RX_CAP = 4096;
    private final byte[] rx = new byte[RX_CAP];
    private int head = 0;   // start of valid data
    private int len  = 0;   // bytes of valid data

    private void receiveFrames(InputStream in) throws IOException {
        final byte SOF = (byte) 0xFF;

        // Read available bytes into ring buffer
        if (space() > 0) {
            int tail = (head + len) % RX_CAP;
            // This is a blocking read. It will wait for data or timeout (1s).
            // This prevents a busy-wait loop when no data is available.
            System.out.println("waiting for data..."); 
            int bytesRead = in.read(rx, tail, Math.min(space(), RX_CAP - tail));
            System.out.println("read returned with " + bytesRead + " bytes");

            if (bytesRead > 0) {
                if (logFileStream != null) {
                    try { logFileStream.write(rx, tail, bytesRead); }
                    catch (IOException e) { System.err.println("Failed to write to log file: " + e.getMessage()); }
                }
                len += bytesRead;
            } else if (bytesRead == -1) {
                throw new IOException("Serial port stream ended.");
            }
        }

        // Parse log lines and binary frames from the buffer
        while (len > 0) {
            System.out.println("buffer has " + len + " bytes to process");
            // We are primarily looking for a binary frame starting with SOF.
            int sofOff = indexOf(SOF, 0, len);

            if (sofOff < 0) {
                // No SOF found. Let's see if we have any complete text lines to process while we wait.
                int newlineOff = indexOf((byte) '\n', 0, len);
                if (newlineOff >= 0) {
                    // We have a line. It could be a log message or bootloader junk.
                    // We'll print it and discard it.
                    byte[] lineBytes = new byte[newlineOff];
                    copyOut(lineBytes, 0, 0, newlineOff);
                    String line = new String(lineBytes).trim();
                    if (!line.isEmpty()) {
                        System.out.println("[ESP32 Log] " + line);
                    }
                    drop(newlineOff + 1);
                    continue; // Restart loop to process next part of the buffer.
                }

                // No SOF and no complete lines. If buffer is full, it's all un-parsable.
                if (len == RX_CAP) {
                    System.out.println("[SerialBridge] Buffer full with no SOF or newline, discarding.");
                    drop(len);
                }
                break; // Wait for more data
            }

            // SOF was found at sofOff. Discard any prefix before it.
            if (sofOff > 0) {
                byte[] prefixBytes = new byte[sofOff];
                copyOut(prefixBytes, 0, 0, sofOff);
                String prefix = new String(prefixBytes).trim();
                if (!prefix.isEmpty()) {
                    // This will catch the "B (...)" marker and other junk.
                    System.out.println("[ESP32 Log] " + prefix);
                }
                drop(sofOff);
            }

            // Now SOF is at the head. Parse the binary frame.
            // C++ sends: [SOF(1)][LEN(2)][PAYLOAD(N)][CSUM(2)]
            final int MIN_FRAME_LEN = 1 + 2 + 0 + 2; // SOF + len + (empty payload) + checksum
            if (len < MIN_FRAME_LEN) {
                break; // Not enough data for even the smallest frame.
            }

            int payloadLen = (get(1) & 0xFF) | ((get(2) & 0xFF) << 8);

            // The payload from the ESP32 always contains a 16-byte metadata header.
            final int MIN_PAYLOAD_LEN = 16;
            // Max payload is 16B metadata + 255B LoRa packet = 271 bytes.
            final int MAX_PAYLOAD_LEN = 271;
            if (payloadLen < MIN_PAYLOAD_LEN || payloadLen > MAX_PAYLOAD_LEN) {
                System.out.println(String.format(
                        "[SerialBridge warning] Invalid payload length: %d. Expected %d-%d. Discarding SOF.",
                        payloadLen, MIN_PAYLOAD_LEN, MAX_PAYLOAD_LEN));
                drop(1);
                continue;
            }

            int frameTotalLen = 1 + 2 + payloadLen + 2; // SOF + len_bytes + payload + csum_bytes
            if (len < frameTotalLen) {
                // If havent found a complete frame, but the buffer is full.
                // Discard the SOF to try and re-sync.
                if (len == RX_CAP) {
                    System.err.println(String.format(
                        "[SerialBridge] Buffer full with partial frame. Discarding SOF to re-sync. (payloadLen=%d, frameTotalLen=%d, bufferLen=%d)",
                        payloadLen, frameTotalLen, len));
                    drop(1);
                    continue; // Re-run the while loop on the modified buffer
                }
                break; // Not enough data yet, and there's space to read more.
           
            }

            byte[] payload = new byte[payloadLen];
            copyOut(payload, 0, 1 + 2, payloadLen);

            int csumOffset = 1 + 2 + payloadLen;
            int recvChk = (get(csumOffset) & 0xFF) | ((get(csumOffset + 1) & 0xFF) << 8);
            int calcChk = inetChecksum16(payload, 0, payloadLen);

            if (calcChk == recvChk) {
                System.out.println("passed checksum, parsing payload with data: " + Arrays.toString(payload));
                // The entire payload is one unit. Let MessageHandler parse it,
                // which is assumed to consume the whole payload.
                onMessageRecv.accept(payload);
            } else {
                System.out.println(String.format(
                        "[SerialBridge warning] Checksum failed for C++ frame: got=0x%04X expected=0x%04X. Discarding SOF and retrying.",
                        recvChk, calcChk));
                drop(1);
                continue;
            }
            drop(frameTotalLen);
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

        // Sum 16-bit words (LE pairing)
        while (len > 1) {
            int word = (data[i++] & 0xFF) | ((data[i++] & 0xFF) << 8); // low,high
            sum += word;
            len -= 2;
        }

        // Odd trailing byte goes into LOW byte
        if (len == 1) {
            sum += (data[i] & 0xFF);
        }

        // Fold to 16 bits with end-around carry, then one's complement
        sum = (sum >> 16) + (sum & 0xFFFF);
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
        // Use synchronized to prevent race conditions if close() is called from multiple threads.
        synchronized (this) {
            if (!running) {
                return; // Already closed or in the process of closing.
            }
            running = false;
        }
        onStatusChange.accept(false);

        // Closing the port will cause the blocking in.read() to throw an exception,
        // which is caught by the reader thread, allowing it to exit gracefully.
        if (port != null && port.isOpen()) port.closePort();
        // Interrupt the thread in case it's sleeping for any reason.
        if (readerThread != null) readerThread.interrupt();

        try { if (out != null) out.close(); } catch (IOException ignored) {}
        try { if (logFileStream != null) { logFileStream.flush(); logFileStream.close(); } } catch (IOException ignored) {}
    }
}
