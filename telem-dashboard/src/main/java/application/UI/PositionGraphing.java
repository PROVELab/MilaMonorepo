package application.UI;

import com.rinearn.graph3d.RinearnGraph3D;
import com.rinearn.graph3d.RinearnGraph3DOptionItem;
import com.rinearn.graph3d.RinearnGraph3DOptionParameter;

import java.awt.*;
import java.awt.event.KeyEvent;

//1 packet every 250 milliseconds (IMU is not guaranteed every packet)
public class PositionGraphing{
    private final RinearnGraph3D graph3D;
    private final int SIZE = 10000;
    private final double[] xArray;
    private final double[] yArray;
    private final double[] zArray;
    private int writeIndex = 0;
    private int count = 0;
    private volatile boolean graphWindowFocused = false;
    private static volatile PositionGraphing instance;

    public PositionGraphing(RinearnGraph3D graph) {
        this.graph3D = graph;
        this.xArray = new double[SIZE];
        this.yArray = new double[SIZE];
        this.zArray = new double[SIZE];
        configureGraphDefaults();
        setupHotkeys();
    }

    public static void plotIMU(PositionGraphing graph, double posX, double posY, double posZ){
        graph.updateArray(posX, posY, posZ);
        graph.plotSnapshot();
    }

    //Given the arrays of x, y, and z, add the next point into the respective arrays
    private void updateArray(double x, double y, double z) {
        xArray[writeIndex] = x;
        yArray[writeIndex] = y;
        zArray[writeIndex] = z;

        writeIndex = (writeIndex + 1) % SIZE;
        if(count < SIZE){
            count++;
        }
    }

    //copies the valid points of the array into one we can use to plot
    private void plotSnapshot(){
        if(count < 1){
            return;
        }

        double[] x = new double[count];
        double[] y = new double[count];
        double[] z = new double[count];
        //buffer
        int start = (writeIndex - count + SIZE) % SIZE;
        for(int i = 0; i < count; i++){
            int idx = (start + i) % SIZE;
            x[i] = xArray[idx];
            y[i] = yArray[idx];
            z[i] = zArray[idx];
        }
        graph3D.setData(x,y,z);
    }


    private void configureGraphDefaults(){
        //UI formatting stuff
        graph3D.setAsynchronousPlottingEnabled(false);    //should be true for actual implementation, is false for testing
        graph3D.setOptionSelected(RinearnGraph3DOptionItem.LINE, true);
        graph3D.setOptionSelected(RinearnGraph3DOptionItem.POINT, true);

        graph3D.setOptionSelected(RinearnGraph3DOptionItem.GRID, false);
        graph3D.setOptionSelected(RinearnGraph3DOptionItem.SCALE, false);
        graph3D.setMenuVisible(true);

        graph3D.setXRange(0, 100);        //The range is in meters,(Cause that's how the position is
        graph3D.setYRange(0, 100);        //calculated, I think, need to check)
        graph3D.setZRange(0, 100);
        graph3D.setXAutoRangingEnabled(false);
        graph3D.setYAutoRangingEnabled(false);
        graph3D.setZAutoRangingEnabled(false);

    }

    private void setupHotkeys() {
        // Track whether the RINEARN Graph 3D window is currently the active window.
        graph3D.addWindowListener(new java.awt.event.WindowAdapter() {
            @Override
            public void windowActivated(java.awt.event.WindowEvent e) {
                graphWindowFocused = true;
            }

            @Override
            public void windowDeactivated(java.awt.event.WindowEvent e) {
                graphWindowFocused = false;
            }
        });

        KeyboardFocusManager.getCurrentKeyboardFocusManager().addKeyEventDispatcher(new KeyEventDispatcher() {
            @Override
            public boolean dispatchKeyEvent(KeyEvent e) {
                if(!graphWindowFocused){
                    return false;
                }
                if (e.getID() == KeyEvent.KEY_RELEASED) {
                    switch (e.getKeyCode()) {
                        //TODO: (NOT A PRIORITY) make it so that you can't spam the menu buttons cause that looks annoying
                        case KeyEvent.VK_M:
                            graph3D.setMenuVisible(false);
                            break;

                        case KeyEvent.VK_N:
                            graph3D.setMenuVisible(true);
                            break;

                        case KeyEvent.VK_0: //black screen hotkey
                            boolean blackScreen = graph3D.isOptionSelected(RinearnGraph3DOptionItem.BLACK_SCREEN);
                            graph3D.setOptionSelected(RinearnGraph3DOptionItem.BLACK_SCREEN, !blackScreen);
                            break;

                        case KeyEvent.VK_1: //Grid hotkey
                            boolean gridLine = graph3D.isOptionSelected(RinearnGraph3DOptionItem.GRID);
                            graph3D.setOptionSelected(RinearnGraph3DOptionItem.GRID, !gridLine);
                            break;

                        case KeyEvent.VK_2: //Frame hotkey
                            boolean frame = graph3D.isOptionSelected(RinearnGraph3DOptionItem.FRAME);
                            graph3D.setOptionSelected(RinearnGraph3DOptionItem.FRAME, !frame);
                            break;

                        case KeyEvent.VK_3: //Scale hotkey
                            boolean scale = graph3D.isOptionSelected(RinearnGraph3DOptionItem.SCALE);
                            graph3D.setOptionSelected(RinearnGraph3DOptionItem.SCALE, !scale);
                            break;

                        case KeyEvent.VK_4: //flat hotkey
                            boolean flat = graph3D.isOptionSelected(RinearnGraph3DOptionItem.FLAT);
                            graph3D.setOptionSelected(RinearnGraph3DOptionItem.FLAT, !flat);
                            break;


                        case KeyEvent.VK_5: //point hotkey
                            boolean point =  graph3D.isOptionSelected(RinearnGraph3DOptionItem.POINT);
                            if(point){
                                graph3D.setOptionSelected(RinearnGraph3DOptionItem.POINT, false);
                            }
                            else{
                                graph3D.setOptionSelected(RinearnGraph3DOptionItem.POINT, true);
                                RinearnGraph3DOptionParameter radius = new RinearnGraph3DOptionParameter();
                                radius.setPointRadius(2);
                                graph3D.setOptionParameter(radius);
                            }
                            break;
                    }
                }
                return false;
            }
        });
    }

    //For CAN frame stuff
    public static void setInstance(PositionGraphing pg) {
        instance = pg;
    }

    public static PositionGraphing getInstance() {
        return instance;
    }
}