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

import org.jfree.chart.ChartFactory;
import org.jfree.chart.ChartPanel;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.chart.plot.ValueMarker;
import org.jfree.chart.plot.XYPlot;
import org.jfree.chart.renderer.AbstractRenderer;
import org.jfree.chart.axis.NumberAxis;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

// assumes you have these (from the DnD step)
// import telemetry.TelemetryDnD.DataPointRef;
// import telemetry.TelemetryDnD.DataPointTransferable;

public class MainPanel extends JPanel {

    private ChartPanel chartPanel1, chartPanel2, chartPanel3, chartPanel4;
    private static final List<JFreeChart> charts = new ArrayList<>();
    private static final List<JPanel> chartPanels = new ArrayList<>();
    private static List<ChartPanel> chartPanelList = new ArrayList<>();

    // Cache: key -> XYSeries (so we can reuse series in charts & timers)
    // key format: "<nodeName>.<dataName>"
    private final Map<TelemetryLookup.DataKey, XYSeries> seriesByRef = new HashMap<>();

    private static int MAX_ELEMENTS_TO_SHOW = 10;
    private static final double currTime = System.currentTimeMillis();

    private final TelemetryLookup lookup; // keep to resolve indices on drop

public MainPanel(TelemetryLookup lookup, int chartCountVertical, int chartCountHorizontal) {
    this.lookup = lookup;                           // keep a field: private final TelemetryLookup lookup;
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
        ((NumberAxis) plot.getRangeAxis()).setAutoRangeIncludesZero(false);
        plot.getRangeAxis().setLabel(title);

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
                var t = dtde.getTransferable();
                if (!t.isDataFlavorSupported(DataInfoTransferable.FLAVOR)) {
                    dtde.rejectDrop(); return;
                }
                dtde.acceptDrop(DnDConstants.ACTION_COPY);

                DataInfoRef ref = (DataInfoRef) t.getTransferData(DataInfoTransferable.FLAVOR);

                // Build tuple key by IDs (nodeId, frameIndex, dataIndex)
                TelemetryLookup.DataKey key = new TelemetryLookup.DataKey(ref.nodeId(), ref.frameIdx(), ref.dpIdx());

                var dpOpt = lookup.getDataInfoById(key);
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

                if (MainFrame.getMultiStatus()) {
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
                var plot = chart.getXYPlot();
                plot.clearRangeMarkers();
                plot.addRangeMarker(new ValueMarker(dp.minWarning()) {{ setPaint(Color.YELLOW); }});
                plot.addRangeMarker(new ValueMarker(dp.maxWarning()) {{ setPaint(Color.YELLOW); }});
                plot.addRangeMarker(new ValueMarker(dp.minCritical()) {{ setPaint(Color.RED); }});
                plot.addRangeMarker(new ValueMarker(dp.maxCritical()) {{ setPaint(Color.RED); }});

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

    /* 4) Demo data feed (random). 
          If you persist real data, replace this with your data source. 
          Also update your CSV/status-signatures to use DataKey. */
    long startTime = System.currentTimeMillis();
    Random random = new Random();
    javax.swing.Timer timer = new javax.swing.Timer(1000, e -> {
        seriesByRef.forEach((key, series) -> {
            double value = 10 + random.nextGaussian() * 0.5;
            series.add((System.currentTimeMillis() - startTime) / 1000.0, value);

            // OPTIONAL: if you keep CSV or status indicators, update them here.
            // Suggested signatures (change your methods accordingly):
            updateCSV(value, lookup.titleFor(key), currTime);
            lookup.getDataInfoById(key).ifPresent(dp -> SensorSelectionPanel.setStatusIndicator(lookup, key, dp, value));
        });
    });
    timer.start();

    // Keep your theming hook if you have one
    darkenCharts();
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

    public static void lightenCharts() {
        for (JFreeChart chart : charts) {
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
    public static void darkenCharts() {
        for (JFreeChart chart : charts) {
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

    public static void setMaxElementsToShow(int maxElementsToShow) {
        MAX_ELEMENTS_TO_SHOW = maxElementsToShow;
    }

    public static int getMaxElementsToShow() {
        return MAX_ELEMENTS_TO_SHOW;
    }

    public static void updateCharts() {
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
            writer.append(String.valueOf(Math.floorDiv( (long) (System.currentTimeMillis() - MainPanel.currTime), 1000)));
            writer.append(",");
            writer.append(String.valueOf(data));
            writer.append(",\n");
            writer.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}