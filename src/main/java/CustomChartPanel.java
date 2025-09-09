import org.jfree.chart.ChartPanel;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.axis.ValueAxis;
import org.jfree.chart.plot.ValueAxisPlot;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

import javax.swing.*;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

public class CustomChartPanel extends ChartPanel {

    public CustomChartPanel(JFreeChart chart) {
        super(chart);
    }

    @Override
    protected JPopupMenu createPopupMenu(boolean properties, boolean copy, boolean save, boolean print, boolean zoom) {
        JPopupMenu menu = super.createPopupMenu(properties, copy, save, print, zoom);

        JMenuItem sliderMenuItem = new JMenuItem("Slider");
        sliderMenuItem.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent actionEvent) {

                JSlider slider = new JSlider(JSlider.HORIZONTAL, 0, 100, 10);
                slider.setMajorTickSpacing(10);
                slider.setMinorTickSpacing(1);
                slider.setPaintTicks(true);
                slider.setPaintLabels(true);
                slider.setPreferredSize(new Dimension(300, 50));
                JDialog dialog = new JDialog();
                dialog.setTitle("Sensor Range");
                dialog.setLayout(new BorderLayout());
                dialog.add(slider, BorderLayout.CENTER); // Add the slider to the dialog
                dialog.setPreferredSize(new Dimension(500,100));
                dialog.pack();
                dialog.setVisible(true);

                // add listener to slider to update chart
                slider.addChangeListener(new ChangeListener() {
                    @Override
                    public void stateChanged(ChangeEvent changeEvent) {
                        // update chart based on slider value
                        JSlider source = (JSlider)changeEvent.getSource();
                        XYSeries series = ((XYSeriesCollection) getChart().getXYPlot().getDataset()).getSeries(0);
                        series.setMaximumItemCount(source.getValue());
                    }
                });

                dialog.add(slider);
                dialog.pack();
                dialog.setVisible(true);
            }
        });

        menu.add(sliderMenuItem);

        return menu;
    }
}