import java.awt.Image;

import javax.swing.*;
import com.formdev.flatlaf.FlatDarkLaf;
import com.formdev.flatlaf.FlatLightLaf;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

public class MainApp {
    private static boolean isNightMode = true;
    public static void main(String[] args) {
        //Initially have dark mode on
        try {
            UIManager.setLookAndFeel(new FlatDarkLaf());
        } catch (Exception ex) {
            System.err.println("Failed to initialize LaF");
        }

        //Textbook Swing Stuff
        SwingUtilities.invokeLater(() -> {
            MainFrame mainFrame = new MainFrame();
            mainFrame.setSize(800, 600);
            mainFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            mainFrame.setVisible(true);
        });
    }

    //Logic for Light/Dark Mode button
    public static void toggleNightMode() {
        try {
            if (isNightMode) {
                UIManager.setLookAndFeel(new FlatLightLaf());
                MainPanel.lightenCharts();
            } else {
                UIManager.setLookAndFeel(new FlatDarkLaf());
                MainPanel.darkenCharts();
            }
            isNightMode = !isNightMode;
            SwingUtilities.updateComponentTreeUI(MainFrame.getFrames()[0]);
        } catch (Exception ex) {
            System.err.println("Failed to toggle night mode");
        }
    }

    //Getter for night mode
    public static boolean isNightMode() {
        return isNightMode;
    }

    //Fetching/Setting the icon for the Light/Dark Mode button
    public static Icon getNightModeIcon() {
        String iconName = isNightMode ? "/sun_icon.png" : "/moon_icon.png";

        ImageIcon icon = new ImageIcon(
            MainApp.class.getResource(iconName)  // leading "/" means root of resources
        );
        Image image = icon.getImage();
        Image newImage = image.getScaledInstance(30, 30, Image.SCALE_SMOOTH);
        icon = new ImageIcon(newImage);
        return icon;
    }
}
