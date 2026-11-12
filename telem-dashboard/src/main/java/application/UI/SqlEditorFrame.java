package application.UI;

import javax.swing.JFrame;

public class SqlEditorFrame extends JFrame {
    public SqlEditorFrame() {
        super("SQL Editor");
        setContentPane(new SqlEditorPanel());
        setSize(1000, 700);
        setMinimumSize(new java.awt.Dimension(750, 550));
        setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        setLocationByPlatform(true);
        
    }
}
