package presentation;
import java.io.EOFException;
import java.nio.ByteBuffer;
import java.util.Optional;

/**
 * A helper class to read a stream of bytes as a stream of bits.
 * This is a permanent, user-managed file, not auto-generated on every run.
 */
public class BitStream {
    private final ByteBuffer buffer;
    private long bitBuffer;
    private int bitCount;

    public BitStream(ByteBuffer buffer) {
        this.buffer = buffer;
        this.bitBuffer = 0;
        this.bitCount = 0;
    }

    private void ensureBits(int n) {
        while (bitCount < n && buffer.hasRemaining()) {
            bitBuffer |= ((long) (buffer.get() & 0xFF)) << bitCount;
            bitCount += 8;
        }
    }

    public int read(int n) throws EOFException, IllegalArgumentException {
        if (n > 32) {
            throw new IllegalArgumentException("Cannot read more than 32 bits into an int");
        }
        ensureBits(n);
        if (bitCount < n) {
            throw new EOFException("Not enough bits available to read " + n + " bits");
        }
        int result = (int) (bitBuffer & ((1L << n) - 1));
        bitBuffer >>>= n;
        bitCount -= n;
        return result;
    }
    
    public int peek(int n) throws EOFException,IllegalArgumentException {
        if (n > 32) {
            throw new IllegalArgumentException("Cannot peek more than 32 bits into an int");
        }
        ensureBits(n);
        if (bitCount < n) {
            throw new EOFException("Not enough bits available to peek " + n + " bits");
        }
        return (int) (bitBuffer & ((1L << n) - 1));
    }
    
    public boolean hasRemaining() {
        return buffer.hasRemaining() || bitCount > 0;
    }
    
    public void alignToByte() {
        int bitsToDiscard = bitCount % 8;
        if (bitsToDiscard > 0) {
            try{
                read(bitsToDiscard); // Discard result
            } catch (EOFException e) {
                // This should never happen since we're just discarding bits, but if it does, we can ignore it.
                System.out.println("alignToByte failed somehow? " + e.getMessage());
            }
        }
    }

    public Optional<byte[]> readBytes(int numBytes, String name) {
        // This method can now read bytes that are not byte-aligned in the stream.
        byte[] result = new byte[numBytes];
        for (int i = 0; i < numBytes; i++) {
            try {
                // The read(8) method correctly handles reading across byte boundaries.
                result[i] = (byte) read(8);
            } catch (EOFException e) {
                // Not enough bits left in the stream to read the requested number of bytes.
                return Optional.empty();
            }
        }
        return Optional.of(result);
    }

    public Optional<Short> readShort(String name) {
        Optional<byte[]> result = readBytes(2, name);
        // The underlying ByteBuffer is Little Endian, and read() consumes LSBs first.
        // readBytes(2,...) will produce {B0, B1}.
        // To reconstruct the little-endian short (0xB1B0), we do (B1 << 8) | B0.
        return result.map(bytes -> (short) ((bytes[1] & 0xFF) << 8 | (bytes[0] & 0xFF)));
    }

    public int remainingBytes( ) {
        alignToByte();
        return buffer.remaining();
    }

    /**
     * Returns the current reading position in the stream, in bits.
     * @return The number of bits that have been consumed from the start of the underlying buffer.
     */
    public int position() {
        // The total bits read from the byte buffer minus the bits still available in the bit buffer.
        return (this.buffer.position() * 8) - this.bitCount;
    }

    /**
     * Returns the current reading position in the stream, in bytes, rounded up.
     * @return The number of bytes that have been consumed.
     */
    public int positionInBytes() {
        return (position() + 7) / 8;
    }
}