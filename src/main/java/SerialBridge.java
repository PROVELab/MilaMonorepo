import com.fazecast.jSerialComm.SerialPort;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.function.Consumer;

public final class SerialBridge implements AutoCloseable {
    private final SerialPort port;
    private final OutputStream out;
    private Thread readerThread;
    private volatile boolean running = false;

    public SerialBridge(String portName, int baud) throws IOException {
        this.port = SerialPort.getCommPort(portName);
        port.setBaudRate(baud);
        port.setNumDataBits(8);
        port.setParity(SerialPort.NO_PARITY);
        port.setNumStopBits(SerialPort.ONE_STOP_BIT);
        port.setComPortTimeouts(SerialPort.TIMEOUT_NONBLOCKING, 0, 0);
        port.setFlowControl(SerialPort.FLOW_CONTROL_DISABLED);
        port.setDTR();
        port.setRTS();
        if (!port.openPort()) throw new IOException("Failed to open port " + portName);

        try { Thread.sleep(2000); } catch (InterruptedException ignored) {}
        this.out = port.getOutputStream();
    }
    //Optionally also start the reader
    public SerialBridge(String portName, int baud,
                        Consumer<byte[]> onMessageRecv,
                        Consumer<byte[]> onMessageInvalid) throws IOException {
        this(portName, baud);                 // do all open/config
        startReader(onMessageRecv, onMessageInvalid);  // kick off thread
    }

    //Start a task for recieving messages, and calls the callback onMessageRecv
    public void startReader(Consumer<byte[]> onMessageRecv, Consumer<byte[]> onMessageInvalid) {
        if (running) return;
        running = true;
        readerThread = new Thread(() -> {
            System.out.println("running recv thread");
            try (InputStream in = port.getInputStream()) {
                System.out.println("init success");
                while (running) {
                    try {
                        receiveFrames(in, onMessageRecv, onMessageInvalid);
                    } catch (IOException e) {
                        if (running) System.out.println("[SerialBridge error] " + e.getMessage());
                    }
                    Thread.yield();
                }
                System.out.println("reader thread exiting");
            } catch (IOException e) {
                if (running) System.out.println("[SerialBridge error] " + e.getMessage());
            }
        }, "serial-reader");
        readerThread.setDaemon(true);
        readerThread.start();
    }

    /** Non-blocking receiver: scans for SOF(0xFF) and parses at most one frame per call.
 *  Frame: [SOF=0xFF][CHK16][ID32_LE][DATA64_LE]  => 15 bytes total; payload=12 bytes (ID+DATA).
 *  - Reads only up to InputStream.available() bytes (never hard-blocks).
 *  - If a complete frame is in 'accum', validates checksum and delivers payload (12 bytes)
 *    to onMessageRecv.accept(payload). Otherwise returns.
 */
    // private void receiveOneFrame(InputStream in,
    //                             ByteArrayOutputStream accum,
    //                             Consumer<byte[]> onMessageRecv) throws IOException {
    //     final byte SOF = (byte)0xFF;
    //     final int FRAME_AFTER_SOF = 14;     // 2(chk) + 4(id) + 8(data)
    //     final int MIN_FRAME_TOTAL  = 1 + FRAME_AFTER_SOF;

    //     // 1) Pull in whatever bytes are available right now (non-blocking)
    //     // while (true){
    //         int avail = port.bytesAvailable();
    //         if (avail > 0) {
    //             byte[] buf = new byte[Math.min(avail, 512)];
    //             int read = in.read(buf);

    //             if (read > 0) {
    //                 accum.write(buf, 0, read);
    //             }
    //         }

    //     // 2) If we don't even have a possible minimum frame, return
    //     byte[] bytes = accum.toByteArray();
    //     if (bytes.length < MIN_FRAME_TOTAL) return;

    //     // 3) Search for SOF
    //     int sof = -1;
    //     for (int i = 0; i < bytes.length; i++) {
    //         if (bytes[i] == SOF) { sof = i; break; }
    //     }
    //     if (sof < 0) {
    //         // No SOF yet; drop old noise if buffer grows large to avoid unbounded growth
    //         if (bytes.length > 2048) {
    //             accum.reset();
    //         }
    //         return;
    //     }

    //     // 4) Do we have enough bytes after SOF for a full frame?
    //     if (bytes.length - sof < MIN_FRAME_TOTAL) {
    //         // Not enough yet; keep bytes and return
    //         return;
    //     }

    //     // 5) Extract the 14-byte block after SOF
    //     int frameStart = sof + 1;
    //     int frameEndExclusive = frameStart + FRAME_AFTER_SOF; // exclusive
    //     byte[] frame = java.util.Arrays.copyOfRange(bytes, frameStart, frameEndExclusive);

    //     // checksum on data 
    //     int chk16 = (frame[0] & 0xFF) | ((frame[1] & 0xFF) << 8);

    //     byte[] payload = java.util.Arrays.copyOfRange(frame, 2, 14); // 12 bytes: ID(4LE)+DATA(8LE)

    //     // 6) Verify checksum (16-bit Internet checksum over payload)
    //     int calc16 = inetChecksum16(payload, 0, payload.length);

    //     // 7) If good, deliver payload and drop consumed bytes up to end of frame
    //     if (chk16 == calc16) {
    //         onMessageRecv.accept(payload);

    //         // drop [0 .. frameEndExclusive) from buffer
    //         ByteArrayOutputStream next = new ByteArrayOutputStream(Math.max(64, bytes.length - frameEndExclusive));
    //         next.write(bytes, frameEndExclusive, bytes.length - frameEndExclusive);
    //         accum.reset();
    //         accum.write(next.toByteArray(), 0, next.size());
    //         return; // process at most one frame per call (stay responsive)
    //     }

    //     // 8) Bad checksum: warn, then resync within this just-read frame window
    //     System.out.print(String.format(
    //         "[SerialBridge warning] checksum failed: got=0x%04X expected=0x%04X.",
    //         chk16, calc16));
    //                 //Assume litte-endian. Arduino is little-endian.
    //     ByteBuffer bb = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
    //     int id   = bb.getInt();
    //     long data = bb.getLong();
    //     final int CanIdMask = 0b1111111;
    //     final int functionCodeMask = 0b1111 << 7;
    //     final int extendedIdMask = 0x3FFFF << 11;

    //     final int nodeId = (id & CanIdMask);
    //     final int functionCode = (id & functionCodeMask) >> 7;
    //     final int extendedId = (id & extendedIdMask) >> 11;
    //     //Debug message in serial may help if getting clapped:
    //     System.out.println(String.format("Invalid CAN frame: id=0x%08X func=0x%08X ext=0x%08X data=0x%016X", nodeId, functionCode, extendedId, data));

    //     // Scan inside the 14-byte 'frame' (after initial SOF) for another SOF
    //     int nextSOF = -1;
    //     for (int k = 0; k < frame.length; k++) {
    //         if (frame[k] == SOF) { nextSOF = k; break; }
    //     }

    //     if (nextSOF >= 0) {
    //         // Keep bytes starting at that inner SOF (relative to original buffer)
    //         int keepFrom = frameStart + nextSOF; // position of inner SOF
    //         ByteArrayOutputStream next = new ByteArrayOutputStream(Math.max(64, bytes.length - keepFrom));
    //         next.write(bytes, keepFrom, bytes.length - keepFrom);
    //         accum.reset();
    //         accum.write(next.toByteArray(), 0, next.size());
    //     } else {
    //         // Drop the entire examined window [0 .. frameEndExclusive)
    //         ByteArrayOutputStream next = new ByteArrayOutputStream(Math.max(64, bytes.length - frameEndExclusive));
    //         next.write(bytes, frameEndExclusive, bytes.length - frameEndExclusive);
    //         accum.reset();
    //         accum.write(next.toByteArray(), 0, next.size());
    //     }
    //     // Return after one attempt; caller loop remains responsive to user input
    // }
    // State

    // Circular buffer state to recv messages
    private static final int RX_CAP = 256;
    private final byte[] rx = new byte[RX_CAP];
    private int head = 0;   // start of valid data
    private int len  = 0;   // bytes of valid data

    private void receiveFrames(InputStream in,
                            Consumer<byte[]> onMessageRecv,
                            Consumer<byte[]> onMessageInvalid) throws IOException {
        final byte SOF = (byte)0xFF;
        final int FRAME_AFTER_SOF = 14;           // 2 + 4 + 8
        final int MIN_FRAME_TOTAL = 1 + FRAME_AFTER_SOF; // 15

        // read available bytes into ring (may wrap)
        int avail = port.bytesAvailable();
        if (avail > 0 && space() > 0) {
            int tail = (head + len) % RX_CAP;
            int toRead = Math.min(avail, space());
            int c1 = Math.min(toRead, RX_CAP - tail);
            int r1 = in.read(rx, tail, c1);
            if (r1 > 0) { len += r1; toRead -= r1; }
            if (toRead > 0) {
                int r2 = in.read(rx, 0, Math.min(toRead, RX_CAP - len));
                if (r2 > 0) { len += r2; }
            }
        }

        // parse as many frames as possible
        while (len >= MIN_FRAME_TOTAL) {
            int sofOff = indexOf(SOF, 0, len);
            if (sofOff < 0) {
                if (len > 128) { head = 0; len = 0; }
                break;
            }
            if (len - sofOff < MIN_FRAME_TOTAL) break;

            int c0 = get(sofOff + 1) & 0xFF;
            int c1 = get(sofOff + 2) & 0xFF;
            int chk16 = c0 | (c1 << 8);

            byte[] payload = new byte[12];                 // ID(4 LE) + DATA(8 LE)
            copyOut(payload, 0, sofOff + 3, 12);
            int calc16 = inetChecksum16(payload, 0, 12);

            int consume;    //how many bytes we consumed with this msg
            if (chk16 == calc16) {
                onMessageRecv.accept(payload);  //callback for valid message :)
                consume = sofOff + MIN_FRAME_TOTAL;
            } else {
                //Indicate checksum issue, and call invalid msg callback
                System.out.print(String.format(
                    "[SerialBridge warning] checksum failed: got=0x%04X expected=0x%04X.",
                    chk16, calc16));
                onMessageInvalid.accept(payload);

                int next = indexOf(SOF, sofOff + 1, MIN_FRAME_TOTAL - 1);
                consume = (next >= 0) ? next : (sofOff + MIN_FRAME_TOTAL);
            }
            drop(consume);
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

    // Send a  message of 8 data byes. 1 byte SOF, 2 byte Checksum, 8 byte data
    public void sendMessage(byte[] data) throws IOException {
        if (data == null) {
            System.out.println("ignoring null msg ");
            return;
        }
        byte[] newData = new byte[8];
        if(data.length != 8){   //pad with 0's
            System.out.println("data length not 8, padding with 0's");
            System.arraycopy(data, 0, newData, 0, Math.min(data.length, 8));
            data = newData;
        }

        // Internet checksum over the 8-byte payload
        int chk16 = inetChecksum16(data, 0, 8) & 0xFFFF;

        // Build frame: [SOF=0xFF][CHK16 BE][DATA 8 bytes]
        byte[] frame = new byte[1 + 2 + 8];
        frame[0] = (byte) 0xFF;
        frame[1] = (byte)(chk16 & 0xFF);
        frame[2] = (byte)(chk16 >>> 8);  
        System.arraycopy(data, 0, frame, 3, 8);

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
