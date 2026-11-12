package application.UI;

import org.jfree.chart.ChartFactory;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.chart.ChartPanel;
import org.jfree.data.xy.XYSeries;
import org.jfree.data.xy.XYSeriesCollection;

import javax.swing.*;
import java.awt.*;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.ResultSet;
import java.sql.PreparedStatement;
import java.util.concurrent.ExecutionException;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.LinkedHashMap;

import java.sql.ResultSetMetaData;
import javax.swing.table.DefaultTableModel;


public class SqlEditorPanel extends JPanel {
    //Database connection URL
    private static final String DATABASE_URL = "jdbc:sqlite:telem_dashboard.db";
    private static final String DEFAULT_TABLE = "telemetry_readings_seconds";
    private static final int MAX_ROWS = 1000;
    //Run Button
    private final JButton runQueryButton = new JButton("Run Query");
    private final JComboBox<String> queryMode = new JComboBox<>(new String[]{"Guided Query","Custom SQL","Statistics"});
    //Toolbar
    private final JLabel statusLabel = new JLabel();
    //results tabs
    private final JTabbedPane resultsTabs = new JTabbedPane();
    private int queryNumber;
    //Guided query inputs
    private final JTextField selectField = new JTextField(" time_seconds, value", 18);
    private final JTextField fromField = new JTextField("telemetry_readings_seconds");
    private final JTextField sessionIdField = new JTextField();
    private final JTextField nodeIdField = new JTextField();
    private final JTextField frameIndexField = new JTextField();
    private final JTextField dataIndexField = new JTextField();
    private final JTextField startField = new JTextField();
    private final JTextField endField = new JTextField();
    //Advanced query inputs
    private final JTextField whereField = new JTextField();
    private final JTextField orderByField = new JTextField("time_seconds ASC");

    //Custom SQL queries
    private final JTextArea customSqlQueryArea = new JTextArea(8, 25);
    private final JScrollPane customQueryScrollPane = new JScrollPane(customSqlQueryArea);
    
    //statistics inputs
    private final JComboBox<String> statsChoiceField = new JComboBox<>(new String[]{"Row Count","Average Value","Minimum Value","Maximum Value" });
    private final JTextField statsFromField = new JTextField("telemetry_readings_seconds");
    private final JTextField statsSessionIdField = new JTextField();
    private final JTextField statsNodeIdField = new JTextField();
    private final JTextField statsFrameIndexField = new JTextField();
    private final JTextField statsDataIndexField = new JTextField();
    private final JTextField statsColumnField = new JTextField("value");
    private final JTextField statsWhereField = new JTextField("");
    public SqlEditorPanel()
    {
        super (new BorderLayout(10,10));
        setBorder(
            BorderFactory.createEmptyBorder(10,10,10,10)
        );
        //placeholder text for guided query inputs
        sessionIdField.putClientProperty("JTextField.placeholderText", "Insert session ID here");
        nodeIdField.putClientProperty("JTextField.placeholderText", "Insert node ID here");
        frameIndexField.putClientProperty("JTextField.placeholderText", "Insert frame index here");
        dataIndexField.putClientProperty("JTextField.placeholderText", "Insert data index here");
        startField.putClientProperty("JTextField.placeholderText", "Insert start time here");
        endField.putClientProperty("JTextField.placeholderText", "Insert end time here");
        whereField.putClientProperty("JTextField.placeholderText", "OPTIONAL: value > 100");
        orderByField.putClientProperty("JTextField.placeholderText", "OPTIONAL: time_seconds ASC");
        //stats text field
        statsWhereField.putClientProperty("JTextField.placeholderText", "OPTIONAL: value > 100");
    
    //Left column section of the SQL editor panel
        JPanel queryColumnPanel = new JPanel(new GridLayout(3,1,0,10));
        queryColumnPanel.setPreferredSize(new Dimension(315, 550));

        JPanel guidedPanel = new JPanel(new BorderLayout());
        guidedPanel.setBorder(BorderFactory.createTitledBorder("Guided Query"));

        JPanel guidedFields = new JPanel(new GridLayout(0,1,0,6));
        guidedFields.add(createRow("SELECT",selectField));
        guidedFields.add(createRow("FROM", fromField));
        guidedFields.add(createRow("session_id", sessionIdField));
        guidedFields.add(createRow("node_id", nodeIdField));
        guidedFields.add(createRow("frame_index", frameIndexField));
        guidedFields.add(createRow("data_index", dataIndexField));
        guidedFields.add(createRow("START (s)", startField));
        guidedFields.add(createRow("END (s)", endField));
        guidedFields.add(createRow("WHERE", whereField));
        guidedFields.add(createRow("ORDER BY", orderByField));
        JScrollPane guidedScrollPane = new JScrollPane(guidedFields);
        guidedScrollPane.getVerticalScrollBar().setUnitIncrement(16);
        guidedScrollPane.getHorizontalScrollBar().setUnitIncrement(16);
        guidedPanel.add(guidedScrollPane, BorderLayout.CENTER);
        
        
        //custom SQL Query section
        JPanel customPanel = new JPanel(new BorderLayout());
        customPanel.setBorder(BorderFactory.createTitledBorder("Custom SQL Query"));
        // maybe add line numbers here


        customSqlQueryArea.setLineWrap(true);
        customSqlQueryArea.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 14));
        customQueryScrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED);
        customQueryScrollPane.setHorizontalScrollBarPolicy(JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
        customQueryScrollPane.getVerticalScrollBar().setUnitIncrement(16);
        customQueryScrollPane.getHorizontalScrollBar().setUnitIncrement(16);
        customPanel.add(customQueryScrollPane, BorderLayout.CENTER);

        //stats section
        JPanel statsPanel = new JPanel(new BorderLayout());
        statsPanel.setBorder(BorderFactory.createTitledBorder("Statistics Query"));


        //stats text field
        statsSessionIdField.putClientProperty("JTextField.placeholderText", "Insert session ID here");
        statsNodeIdField.putClientProperty("JTextField.placeholderText", "Insert node ID here");
        statsFrameIndexField.putClientProperty("JTextField.placeholderText", "Insert frame index here");
        statsDataIndexField.putClientProperty("JTextField.placeholderText", "Insert data index here");
        statsWhereField.putClientProperty("JTextField.placeholderText", "OPTIONAL: value > 100");
        
        
        JPanel statsFields = new JPanel(new GridLayout(0,1,0,6));

        JScrollPane statsScrollPane = new JScrollPane(statsFields);
        statsScrollPane.getVerticalScrollBar().setUnitIncrement(16);
        statsScrollPane.getHorizontalScrollBar().setUnitIncrement(16);
        
        statsFields.add(createRow("Statistics", statsChoiceField));
        statsFields.add(createRow("Table", statsFromField));
        statsFields.add(createRow("session_id", statsSessionIdField));
        statsFields.add(createRow("node_id", statsNodeIdField));
        statsFields.add(createRow("frame_index", statsFrameIndexField));
        statsFields.add(createRow("data_index", statsDataIndexField));
        statsFields.add(createRow("Column", statsColumnField));
        statsFields.add(createRow("Filter", statsWhereField));
        statsPanel.add(statsScrollPane, BorderLayout.CENTER);
    
        
        queryColumnPanel.add(guidedPanel);
        queryColumnPanel.add(customPanel);
        queryColumnPanel.add(statsPanel);
        add(queryColumnPanel, BorderLayout.WEST);
        // Run Query Button
        
        runQueryButton.setBackground(new Color(46,125,50));
        runQueryButton.setForeground(Color.BLACK);
        JPanel toolbar = new JPanel(new FlowLayout(FlowLayout.LEFT, 8, 4));
        toolbar.add(queryMode);
        toolbar.add(runQueryButton);
        toolbar.add(statusLabel);
        // for future toolbar buttons

        // graphing results tabs
        resultsTabs.setTabLayoutPolicy(JTabbedPane.SCROLL_TAB_LAYOUT);
        JPanel resultsPanel = new JPanel(new BorderLayout(0,8));
        resultsPanel.add(resultsTabs, BorderLayout.CENTER);
        resultsPanel.add(toolbar, BorderLayout.NORTH);
        add(resultsPanel, BorderLayout.CENTER);
        runQueryButton.addActionListener(e -> runQuery());
        
    }
    // Builds SQL using selected query mode
    private QueryRequest buildQuery(){
        // for custom SQL
        String mode = (String) queryMode.getSelectedItem();
        if ("Custom SQL".equals(mode))
        {
            String sql = customSqlQueryArea.getText().trim();
            if (sql.isEmpty())
            {
                throw new IllegalArgumentException("Please enter a query in Custom SQL");
            }
            return new QueryRequest(mode, sql, List.of(), false);
        }
        // guided/statistics filters
        boolean statistics = "Statistics".equals(mode);
        List<String> filters = new ArrayList<>();
        List<Object> parameters = new ArrayList<>();
        addFilter(filters, parameters, "session_id", statistics ? statsSessionIdField: sessionIdField, false);
        addFilter(filters, parameters, "node_id", statistics ? statsNodeIdField: nodeIdField, true);
        addFilter(filters, parameters, "frame_index", statistics ? statsFrameIndexField: frameIndexField, true);
        addFilter(filters, parameters, "data_index", statistics ? statsDataIndexField: dataIndexField, true);
        if (!statistics)
        {
            Double start = seconds(startField, "start");
            Double end = seconds(endField, "end");
            if (start != null && end != null && end <= start)
            {
                throw new IllegalArgumentException("End must be greater than start");
            }
            if (start != null)
            {
            filters.add("time_seconds >= ?");
            parameters.add(start);
            }
            if (end != null)
            {
            filters.add("time_seconds <= ?");
            parameters.add(end);
            }
        }
        
        String extra = (statistics ? statsWhereField : whereField).getText().trim();
        if (!extra.isEmpty())
        {
            filters.add("(" + extra + ")");
        }
        String selection;
        if (statistics)
        {
            String column = statsChoiceField.getSelectedIndex() == 0 ? "": identifier (textOrDefault(statsColumnField,"value"));
            selection = switch (statsChoiceField.getSelectedIndex())
            {
                case 1 -> "AVG(" + column + ") AS average_value";
                case 2 -> "MIN(" + column + ") AS minimum_value";
                case 3 -> "MAX(" + column + ") AS maximum_value";
                default -> "COUNT(*) AS reading_count";
            };
        }
        else
        {
            selection = textOrDefault(selectField, "time_seconds,value");
        }
        String table = identifier(textOrDefault(statistics ? statsFromField: fromField, DEFAULT_TABLE));
        String sql = "SELECT " + selection + " FROM " + table;
        if (!filters.isEmpty())
        {
            sql += " WHERE " + String.join(" AND ", filters);
        }
        if (!statistics && !orderByField.getText().isBlank())
        {
            sql += " ORDER BY " + orderByField.getText().trim();
        }
        boolean singleSensor = !statistics && !sessionIdField.getText().isBlank() && !nodeIdField.getText().isBlank() && !frameIndexField.getText().isBlank() && !dataIndexField.getText().isBlank();
        return new QueryRequest(mode, sql, List.copyOf(parameters), singleSensor);
        

    }
    
    private void runQuery()
    {
        if (!runQueryButton.isEnabled())
        {
            return;
        }
        final QueryRequest request;
        try
        {
            request = buildQuery();
        }
        catch (IllegalArgumentException error)
        {
            JOptionPane.showMessageDialog(this, error.getMessage(), "Check Query Inputs", JOptionPane.ERROR_MESSAGE);
            return;
        }
        runQueryButton.setEnabled(false);
        queryMode.setEnabled(false);
        statusLabel.setText("Running " + request.mode() + "...");
        SwingWorker<QueryResult, Void> worker = new SwingWorker<>()
        {
            @Override
            protected QueryResult doInBackground() throws SQLException
            {
                return executeQuery(request);
            }
            @Override 
            protected void done()
            {
                try
                {
                    showResult(request,get());
                }
                catch(InterruptedException Error)
                {
                    Thread.currentThread().interrupt();
                    statusLabel.setText("Query interrupted");
                }
                catch (ExecutionException error)
                {
                    statusLabel.setText("Query failed: " + error.getCause().getMessage());
                    JOptionPane.showMessageDialog(SqlEditorPanel.this, error.getCause().getMessage(), "Database Query Failed", JOptionPane.ERROR_MESSAGE);
                }
                finally
                {
                    runQueryButton.setEnabled(true);
                    queryMode.setEnabled(true);
                }
            }
        };
        worker.execute();
    }
    private QueryResult executeQuery(QueryRequest request) throws SQLException
    {
        // query execution logic
        List<String> columns = new ArrayList<>();
        List<Object[]> rows = new ArrayList<>();
        boolean truncated = false;
        try(Connection connection = DriverManager.getConnection(DATABASE_URL);PreparedStatement statement = connection.prepareStatement(request.sql()))
        {
            for (int i = 0; i < request.parameters().size(); i++)
            {
                statement.setObject(i + 1, request.parameters().get(i));
            }
            statement.setMaxRows(MAX_ROWS + 1);
            try (ResultSet results = statement.executeQuery())
            {
                ResultSetMetaData metadata = results.getMetaData();
                int columnCount = metadata.getColumnCount();
                for (int i = 1; i <= columnCount; i++)
                {
                    columns.add(metadata.getColumnLabel(i));
                }
                while (results.next())
                {
                    if (rows.size() == MAX_ROWS)
                    {
                        truncated = true;
                        break;
                    }
                    Object[] row = new Object[columnCount];
                    for (int i = 0; i < columnCount; i++)
                    {
                        row[i] = results.getObject(i+1);
                    }
                    rows.add(row);
                }
            }
        }
        JFreeChart chart = createChart(request,columns,rows);
        return new QueryResult(columns, rows, truncated, chart);
    }
    private JFreeChart createChart(QueryRequest request, List<String> columns, List<Object[]> rows)
    {
        if ("Statistics".equals(request.mode())|| rows.isEmpty())
        {
            return null;
        }
        int x = columnIndex(columns,"time_seconds");
        String xLabel = "Time (seconds)";
        if (x < 0)
        {
            x = columnIndex(columns, "time_ms");
            xLabel = "Time (milliseconds)";
        }
        int y = columnIndex(columns, "value");
        if (x < 0 || y <0)
        {
            return null;
        }
        int session = columnIndex(columns, "session_id");
        int node = columnIndex(columns, "node_id");
        int frame = columnIndex(columns, "frame_index");
        int data = columnIndex(columns, "data_index");
        boolean identified = session >= 0 && node >= 0 && frame >= 0 && data >= 0;
        Map<String, XYSeries> seriesBySensor = new LinkedHashMap<>();
        for (Object[] row : rows)
        {
            if (!(row[x] instanceof Number time) || !(row[y] instanceof Number value))
            {
                continue;
            }
            double timeValue = time.doubleValue();
            double sensorValue = value.doubleValue();
            if (!Double.isFinite(timeValue) || !Double.isFinite(sensorValue))
            {
                continue;
            }
            String key = identified
                ? row[session] + "/" + row[node] + "/" + row[frame] + "/" + row[data]
                : "Query Result";
            XYSeries series = seriesBySensor.computeIfAbsent(key, name -> new XYSeries(name, true, true));
            series.add(timeValue, sensorValue);
            
        }
        if (seriesBySensor.isEmpty())
        {
            return null;
        }
        XYSeriesCollection dataset = new XYSeriesCollection();
        seriesBySensor.values().forEach(dataset::addSeries);
        if (identified || request.singleSensor())
        {
            return ChartFactory.createXYLineChart("Query Result", xLabel, "Value", dataset, PlotOrientation.VERTICAL, true, true, false);
        }
        return ChartFactory.createScatterPlot("Query Result", xLabel, "Value", dataset, PlotOrientation.VERTICAL, true, true, false);

    }
    
    private void showResult(QueryRequest request, QueryResult result)
    {
        JComponent content;
        if (result.chart() != null)
        {
            content = new ChartPanel(result.chart());
        }
        else
        {
            DefaultTableModel model = new DefaultTableModel(result.rows().toArray(new Object[0][]), result.columns().toArray())
            {
                @Override 
                public boolean isCellEditable(int row, int column)
                {
                    return false;
                }
            };
            content = new JScrollPane(new JTable(model));

        }
        String message = result.truncated() ? "showing first " + MAX_ROWS + " rows; narrow the query for a smaller result." : result.rows().size() + " rows returned.";
        JPanel page = new JPanel(new BorderLayout());
        page.add(content, BorderLayout.CENTER);
        String title = request.mode() + " " + (++queryNumber);
        resultsTabs.addTab(title, page);
        int tabIndex = resultsTabs.indexOfComponent(page);
        resultsTabs.setTabComponentAt(tabIndex, createTabHeader(title, page));
        resultsTabs.setToolTipTextAt(tabIndex, message);
        resultsTabs.setSelectedComponent(page);
        statusLabel.setText(message);
    }

    private static void addFilter(List<String> filters, List<Object> parameters, String column, JTextField field, boolean numeric)
    {
        String text = field.getText().trim();
        if (text.isEmpty())
        {
            return;
        }
        Object value = text;
        if (numeric)
        {
            try
            {
                int number = Integer.parseInt(text);
                if (number < 0)
                {
                    throw new NumberFormatException();
                }
                value = number;

            }
            catch (NumberFormatException e)
            {
                throw new IllegalArgumentException(column + " must be a non-negative integer.");
            }
            
        }
        filters.add(column + " = ?");
        parameters.add(value);
    }
    private static Double seconds(JTextField field, String label)
    {
        if (field.getText().isBlank())
        {
            return null;

        }
        try
        {
            double value = Double.parseDouble(field.getText().trim());
            if (!Double.isFinite(value) || value < 0)
            {
                throw new NumberFormatException();
            }
            return value;
        }
        catch (NumberFormatException e)
        {
            throw new IllegalArgumentException(label + " must be a non-negative number of seconds.");
        }
    }
    private static String identifier(String name)
    {
        if (!name.matches("[A-Za-z_][A-Za-z0-9_]*"))
        {
            throw new IllegalArgumentException("Invalid identifier: " + name);
        }
        return "[" + name + "]";
    }
    private static String textOrDefault(JTextField field, String fallback)
    {
        String text = field.getText().trim();
        return text.isEmpty() ? fallback : text;

    }
    private static int columnIndex(List<String> columns, String name)
    {
        for (int i = 0; i < columns.size(); i++)
        {
            if (columns.get(i).equalsIgnoreCase(name))
            {
                return i;
            }
        }
        return -1;
    }
    private JPanel createTabHeader(String title, JPanel page)
    {
        JPanel header = new JPanel(new FlowLayout(FlowLayout.LEFT,5,0));
        header.setOpaque(false);
        JLabel titleLabel = new JLabel(title);
        JButton closeButton = new JButton("×");
        closeButton.setFocusable(false);
        closeButton.setMargin(new Insets(0, 4, 0, 4));
        closeButton.addActionListener(e -> resultsTabs.remove(page));
        header.add(titleLabel);
        header.add(closeButton);
        return header;
    }


    // Creates a row with a label and an input component
    private JPanel createRow(String labelText, JComponent input)
        {
        JPanel row = new JPanel(new BorderLayout(8,0));
        JLabel label = new JLabel(labelText);
        label.setPreferredSize(new Dimension(90,24));   
        row.add(label, BorderLayout.WEST);
        row.add(input, BorderLayout.CENTER);
        return row;
    }
    private record QueryRequest(String mode, String sql, List<Object> parameters, boolean singleSensor)
    {

    }
    private record QueryResult(List<String> columns, List<Object[]> rows, boolean truncated, JFreeChart chart)
    {

    }
    

}
