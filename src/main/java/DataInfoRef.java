
import java.awt.datatransfer.*;
import java.io.IOException;

//Transferable allows us to associate charts/buttons with their corresponding lookup info
// Payload: just indices (+ nodeName for display)
public record DataInfoRef(String nodeName, int nodeId, int frameIdx, int dpIdx) {}

final class DataInfoTransferable implements Transferable {
    public static final DataFlavor FLAVOR;
    static {
        try {
            FLAVOR =
                new DataFlavor(DataFlavor.javaJVMLocalObjectMimeType + ";class=" + DataInfoRef.class.getName());
        } catch (ClassNotFoundException e) {
            throw new RuntimeException("Class not found: " + DataInfoRef.class.getName(), e);
        }
    }
    private static final DataFlavor[] FLAVORS = { FLAVOR };

    private final DataInfoRef ref;

    public DataInfoTransferable(DataInfoRef ref) { this.ref = ref; }

    @Override public DataFlavor[] getTransferDataFlavors() { return FLAVORS; }
    @Override public boolean isDataFlavorSupported(DataFlavor flavor) { return FLAVOR.equals(flavor); }
    @Override public Object getTransferData(DataFlavor flavor) throws UnsupportedFlavorException, IOException {
        if (!isDataFlavorSupported(flavor)) throw new UnsupportedFlavorException(flavor);
        return ref;
    }
}