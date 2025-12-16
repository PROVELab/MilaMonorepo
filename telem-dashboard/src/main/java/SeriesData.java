import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

class SeriesData{
    private XYSeriesCollection xysc;
    private XYSeries ser;

    public SeriesData(XYSeriesCollection xysc, XYSeries ser) {
        this.xysc = xysc;
        this.ser = ser;
    }

    public XYSeriesCollection getXYSeriesCollection() {
        return xysc;
    }

    public XYSeries getXYSeries() {
        return ser;
    }
}

