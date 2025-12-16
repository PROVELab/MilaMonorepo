import java.awt.*;
import java.awt.datatransfer.DataFlavor;
import java.awt.dnd.*;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.NumberFormat;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;
import javax.swing.*;
import javax.swing.border.LineBorder;

import org.jfree.data.Range;
import org.jfree.chart.ChartFactory;
import org.jfree.chart.ChartPanel;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.chart.plot.ValueMarker;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.AbstractRenderer;
import org.jfree.chart.ui.Layer;
import org.jfree.chart.axis.NumberAxis;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

// The panel displaying the charts of data
public class MainPanel extends JPanel {

    private final List<JFreeChart> charts = new ArrayList<>();
    private final List<JPanel> chartPanels = new ArrayList<>();
    private List<ChartPanel> chartPanelList = new ArrayList<>();

    private final Map<TelemetryLookup.DataKey, XYSeries> seriesByRef = new HashMap<>();

    private int MAX_ELEMENTS_TO_SHOW = 10;   //set default number of data displayed to 10, can be updated with slider
    private static final double startTime = System.currentTimeMillis();

    private final TelemetryLookup lookup; 
    private MainFrame mainFrame = null;

public MainPanel(TelemetryLookup lookup, int chartCountVertical, int chartCountHorizontal) {
    this.lookup = lookup;
    setLayout(new GridLayout(chartCountVertical, chartCountHorizontal));

    /* 1) Build one XYSeries per DataKey */
    for (TelemetryLookup.DataKey key : lookup.allDataKeys()) {
        String title = lookup.titleFor(key);        // "<nodeName>.<dataName>"
        XYSeries series = new XYSeries(title);
        series.setMaximumItemCount(MAX_ELEMENTS_TO_SHOW);
        seriesByRef.put(key, series);
    }

    /* 2) Create the grid of charts*/
    final int numCharts = chartCountVertical * chartCountHorizontal;
    int i = 0;
    for (var entry : seriesByRef.entrySet()) {
        if (i >= numCharts) break;

        TelemetryLookup.DataKey key = entry.getKey();
        XYSeries series = entry.getValue();

        String title = lookup.titleFor(key);
        XYSeriesCollection dataset = new XYSeriesCollection(series);
        JFreeChart chart = createChart(dataset, title);

        // Style chart once at creation
        var plot = chart.getXYPlot();
        plot.getRendererForDataset(plot.getDataset()).setSeriesPaint(0, Color.WHITE);
        plot.getRenderer().setDefaultStroke(new BasicStroke(4.0f));
        ((AbstractRenderer) plot.getRenderer()).setAutoPopulateSeriesStroke(false);
        plot.getDomainAxis().setLabel("Time (s)");
        NumberAxis yAxis = (NumberAxis) plot.getRangeAxis();
        Optional<TelemetryLookup.DataInfo> inf= lookup.getDataInfo(key);
        if(inf.isEmpty()){
            System.out.println("unable to find dataInfo for a chart that needs to be displayed! Quitting");
            System.exit(1);
        }
        int minimumSize = Math.max(Math.min( 1  << (inf.get().bitLength()>>1), 2), 100);
        yAxis.setAutoRangeIncludesZero(false);   // dont force include zero
        yAxis.setAutoRangeMinimumSize(minimumSize);     // enforce at least 2 units tall
        plot.getRangeAxis().setLabel(title);
        applyThresholdMarkers(chart, key);


        charts.add(chart);

        ChartPanel cp = new CustomChartPanel(chart);
        cp.setDomainZoomable(false);
        cp.setRangeZoomable(false);
        cp.setHorizontalAxisTrace(false);
        cp.setVerticalAxisTrace(false);
        cp.setBorder(new LineBorder(Color.BLACK));

        chartPanelList.add(cp);
        chartPanels.add(cp);
        add(cp);

        i++;
    }

    /* 3) Drop handler: accept DataInfoTransferable, resolve via tuple key, update chart */
    DropTargetListener dtl = new DropTargetAdapter() {
        @Override
        public void drop(DropTargetDropEvent dtde) {
            try {
                var t = dtde.getTransferable(); //Transferable allows us to associate the dataKey with corresponding chart in on click events
                if (!t.isDataFlavorSupported(DataInfoTransferable.FLAVOR)) {
                    dtde.rejectDrop(); return;
                }
                dtde.acceptDrop(DnDConstants.ACTION_COPY);

                DataInfoRef ref = (DataInfoRef) t.getTransferData(DataInfoTransferable.FLAVOR);

                // Build tuple key by IDs (nodeId, frameIndex, dataIndex)
                TelemetryLookup.DataKey key = new TelemetryLookup.DataKey(ref.nodeId(), ref.frameIdx(), ref.dpIdx());

                var dpOpt = lookup.getDataInfo(key);
                if (dpOpt.isEmpty()) { dtde.dropComplete(false); return; }
                TelemetryLookup.DataInfo dp = dpOpt.get();

                XYSeries ser = seriesByRef.get(key); // series was created in step (1)
                if (ser == null) { // shouldn't happen, but handle defensively
                    ser = new XYSeries(lookup.titleFor(key));
                    ser.setMaximumItemCount(MAX_ELEMENTS_TO_SHOW);
                    seriesByRef.put(key, ser);
                }

                ChartPanel droppedChartPanel = (ChartPanel) dtde.getDropTargetContext().getComponent();
                JFreeChart chart = droppedChartPanel.getChart();
                if(mainFrame == null){
                    System.out.println("havent connected mainFrame yet. :/");
                }
                if (mainFrame != null && mainFrame.getMultiStatus()) {
                    // Add series to existing dataset
                    XYSeriesCollection dataset = (XYSeriesCollection) chart.getXYPlot().getDataset();
                    dataset.addSeries(ser);
                    chart.setTitle("Multiple Sensors");
                } else {
                    // Replace dataset with a single series
                    XYSeriesCollection dataset = new XYSeriesCollection();
                    dataset.addSeries(ser);
                    chart.getXYPlot().setDataset(dataset);
                    chart.setTitle(lookup.titleFor(key));
                }

                // Axes formatting
                ((NumberAxis) chart.getXYPlot().getDomainAxis())
                        .setNumberFormatOverride(NumberFormat.getNumberInstance());
                ((NumberAxis) chart.getXYPlot().getRangeAxis())
                        .setNumberFormatOverride(NumberFormat.getNumberInstance());

                // Threshold markers from DataInfo
                //TODO: test that these work.
                var plot = chart.getXYPlot();
                applyThresholdMarkers(chart, key);

                plot.getRangeAxis().setLabel(lookup.titleFor(key));

                droppedChartPanel.repaint();
                dtde.dropComplete(true);
            } catch (Exception e) {
                e.printStackTrace();
                dtde.dropComplete(false);
            }
        }
    };

    // Attach the DropTarget to each displayed chart panel
    for (ChartPanel cp : chartPanelList) {
        new DropTarget(cp, DnDConstants.ACTION_COPY, dtl, true);
    }

    darkenCharts();
}
    public void connectFrame(MainFrame mainFrame) {
        this.mainFrame=mainFrame;
    }
    private void applyThresholdMarkers(JFreeChart chart, TelemetryLookup.DataKey key) {
        lookup.getDataInfo(key).ifPresent(dp -> {
            XYPlot plot = chart.getXYPlot();
            plot.clearRangeMarkers();

            ValueMarker wMin = new ValueMarker(dp.minWarning());
            wMin.setPaint(Color.YELLOW); wMin.setStroke(new BasicStroke(2f));
            plot.addRangeMarker(wMin, Layer.FOREGROUND);

            ValueMarker wMax = new ValueMarker(dp.maxWarning());
            wMax.setPaint(Color.YELLOW); wMax.setStroke(new BasicStroke(2f));
            plot.addRangeMarker(wMax, Layer.FOREGROUND);

            ValueMarker cMin = new ValueMarker(dp.minCritical());
            cMin.setPaint(Color.RED); cMin.setStroke(new BasicStroke(2f));
            plot.addRangeMarker(cMin, Layer.FOREGROUND);

            ValueMarker cMax = new ValueMarker(dp.maxCritical());
            cMax.setPaint(Color.RED); cMax.setStroke(new BasicStroke(2f));
            plot.addRangeMarker(cMax, Layer.FOREGROUND);
        });

    }
    public boolean addDataPoint(TelemetryLookup.DataKey key, int value){
        if(!seriesByRef.containsKey(key)){
            return false;
        }
        XYSeries updatedSeries = seriesByRef.get(key);
        updatedSeries.add((System.currentTimeMillis() - startTime) / 1000.0, value);
        updateCSV(value, lookup.titleFor(key), startTime);
        return true;
    }
    public boolean addDataPoint(int nodeId, int frameIdx, int dataIdx, int value){
        TelemetryLookup.DataKey key = new TelemetryLookup.DataKey(nodeId, frameIdx, dataIdx);
        return addDataPoint(key, value);
    }

    //Chart Axis Labels and Frame
    private JFreeChart createChart(XYSeriesCollection dataset, String title) {
        return ChartFactory.createXYLineChart(
            title,
            "Time",
            "Value",
            dataset,
            PlotOrientation.VERTICAL,
            true, true, false);
    }

    public void lightenCharts() {
        for (JFreeChart chart : this.charts) {
            chart.getTitle().setPaint(Color.BLACK);
            chart.setBackgroundPaint(Color.WHITE);
            chart.getPlot().setBackgroundPaint(Color.WHITE);
            chart.getLegend().setBackgroundPaint(Color.WHITE);
            chart.getLegend().setItemPaint(Color.DARK_GRAY);
            XYPlot plot = chart.getXYPlot();

            // Change the color of the tick labels on the X axis
            ValueAxis xAxis = plot.getDomainAxis();
            xAxis.setTickLabelPaint(Color.BLACK);
            xAxis.setLabelPaint(Color.BLACK);

            // Change the color of the tick labels on the Y axis
            ValueAxis yAxis = plot.getRangeAxis();
            yAxis.setTickLabelPaint(Color.BLACK);
            yAxis.setLabelPaint(Color.BLACK);

            //Change the color of the first line. The other lines stay the same.
            chart.getXYPlot().getRendererForDataset(chart.getXYPlot().getDataset()).setSeriesPaint(0, Color.DARK_GRAY);
        }
    }
    public void darkenCharts() {
        for (JFreeChart chart : this.charts) {
            chart.getTitle().setPaint(Color.WHITE);
            chart.setBackgroundPaint(Color.DARK_GRAY);
            chart.getPlot().setBackgroundPaint(Color.DARK_GRAY);
            chart.getLegend().setBackgroundPaint(Color.DARK_GRAY);
            chart.getLegend().setItemPaint(Color.WHITE);
            XYPlot plot = chart.getXYPlot();

            // Change the color of the tick labels on the X axis
            ValueAxis xAxis = plot.getDomainAxis();
            xAxis.setTickLabelPaint(Color.WHITE);
            xAxis.setLabelPaint(Color.WHITE);


            // Change the color of the tick labels on the Y axis
            ValueAxis yAxis = plot.getRangeAxis();
            yAxis.setTickLabelPaint(Color.WHITE);
            yAxis.setLabelPaint(Color.WHITE);

            //Change the color of the first line. The other lines stay the same
            chart.getXYPlot().getRendererForDataset(chart.getXYPlot().getDataset()).setSeriesPaint(0, Color.WHITE);
        }
    }

    public void setMaxElementsToShow(int maxElementsToShow) {
        MAX_ELEMENTS_TO_SHOW = maxElementsToShow;
    }

    public int getMaxElementsToShow() {
        return MAX_ELEMENTS_TO_SHOW;
    }

    public void updateCharts() {
        for (JFreeChart chart : charts) {
            for (int i = 0; i < chart.getXYPlot().getDataset().getSeriesCount(); i++) {
                XYSeries series = ((XYSeriesCollection) chart.getXYPlot().getDataset()).getSeries(i);
                series.setMaximumItemCount(MAX_ELEMENTS_TO_SHOW);
            }
        }
    }

    public void updateCSV(double data, String fileName, double currTime){
        try {
            File z = new File("data/");

            if (!z.exists()) {
                z.mkdir();
            }

            File f = new File("data/" + fileName + ".csv");
            if (!f.exists()){
                f.createNewFile();
            }
            FileWriter writer = new FileWriter(f, true);
            writer.append(String.valueOf(Math.floorDiv( (long) (System.currentTimeMillis() - MainPanel.startTime), 1000)));
            writer.append(",");
            writer.append(String.valueOf(data));
            writer.append(",\n");
            writer.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}