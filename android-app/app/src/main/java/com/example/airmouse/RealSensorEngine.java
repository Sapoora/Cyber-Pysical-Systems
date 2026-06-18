package com.example.airmouse;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Trace;

public class RealSensorEngine implements SensorEventListener {

    private final SensorDataListener listener;
    private final SensorManager sensorManager;

    private Sensor accelerometer;
    private Sensor gyroscope;
    private Sensor magnetometer;

    private boolean running = false;

    // --- Calibration Parameters ---
    // Gyroscope Bias (Static compensation)
    public static float[] gyroBias = new float[3]; // X, Y, Z

    // Accelerometer 6-position linear parameters (Corrected = (Raw - Offset) / Scale)
    public static float[] accelOffset = new float[]{0f, 0f, 0f};
    public static float[] accelScale = new float[]{1f, 1f, 1f};

    // Magnetometer Figure-8 parameters
    public static float[] magOffset = new float[]{0f, 0f, 0f};
    public static float[] magScale = new float[]{1f, 1f, 1f};

    // --- Complementary Filter & Orientation State ---
    private float[] orientation = new float[3]; // Roll, Pitch, Yaw
    private long lastTimestamp = 0;
    private static final float FILTER_ALPHA = 0.95f; // Weight for Gyroscope vs Accel/Mag

    // --- Thresholds & Gesture Control ---
    private static final float MOVEMENT_SENSITIVITY = 15.0f;
    private static final float GYRO_Y_CLICK_THRESHOLD = 4.5f; // Rapid rotation around Y-axis for Left Click
    private static final float ACCEL_Y_SCROLL_THRESHOLD = 6.0f; // Rapid acceleration along Y-axis for Scroll

    private long lastClickTime = 0;
    private long lastScrollTime = 0;
    private static final long GESTURE_DEBOUNCE_MS = 400; // Waiting window to avoid hand return mistakes

    public RealSensorEngine(Context context, SensorDataListener listener) {
        this.listener = listener;
        this.sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);

        if (sensorManager != null) {
            this.accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            this.gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            this.magnetometer = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        }
    }

    public void start() {
        if (running) return;
        running = true;
        lastTimestamp = 0;

        if (sensorManager != null) {
            // Registering sensors with SENSOR_DELAY_GAME for high responsiveness
            if (accelerometer != null) sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
            if (gyroscope != null) sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME);
            if (magnetometer != null) sensorManager.registerListener(this, magnetometer, SensorManager.SENSOR_DELAY_GAME);
        }
    }

    public void stop() {
        if (!running) return;
        running = false;
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
    }

    // Temporary storage for raw values needed for the complementary fusion step
    private float[] rawAccel = new float[3];
    private float[] rawMag = new float[3];
    private boolean hasAccel = false;
    private boolean hasMag = false;

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (!running) return;

        // Custom Trace Event for Perfetto profiling
        Trace.beginSection("AirMouse_OnSensorChanged");

        long currentTime = System.currentTimeMillis();

        switch (event.sensor.getType()) {
            case Sensor.TYPE_GYROSCOPE:
                // 1. Apply Gyro Calibration (Subtract static bias average)
                float gyroX = event.values[0] - gyroBias[0];
                float gyroY = event.values[1] - gyroBias[1];
                float gyroZ = event.values[2] - gyroBias[2];

                // Time delta calculation
                if (lastTimestamp != 0) {
                    final float dT = (event.timestamp - lastTimestamp) * 1.0e-9f;

                    // 2. Execute Fusion Filter Layer (Complementary Filter)
                    Trace.beginSection("AirMouse_ComplementaryFilter");

                    // Gyro integration for short-term prediction
                    orientation[0] += gyroX * dT; // Roll
                    orientation[1] += gyroY * dT; // Pitch
                    orientation[2] += gyroZ * dT; // Yaw

                    // Drifting & noise compensation via Accelerometer/Magnetometer long-term absolute reference
                    if (hasAccel && hasMag) {
                        float[] R = new float[9];
                        float[] I = new float[9];
                        if (SensorManager.getRotationMatrix(R, I, rawAccel, rawMag)) {
                            float[] actualOrientation = new float[3];
                            SensorManager.getOrientation(R, actualOrientation);

                            // Apply weighted fusion formula to eliminate drift and jitter
                            orientation[0] = FILTER_ALPHA * orientation[0] + (1.0f - FILTER_ALPHA) * actualOrientation[1]; // Pitch mapping
                            orientation[1] = FILTER_ALPHA * orientation[1] + (1.0f - FILTER_ALPHA) * actualOrientation[2]; // Roll mapping
                            orientation[2] = FILTER_ALPHA * orientation[2] + (1.0f - FILTER_ALPHA) * actualOrientation[0]; // Azimuth mapping
                        }
                    }
                    Trace.endSection(); // AirMouse_ComplementaryFilter

                    // 3. Differential Motion Extraction Algorithm
                    // Projecting Z-axis variations to Horizontal DeltaX and X-axis variations to Vertical DeltaY
                    float deltaX = gyroZ * MOVEMENT_SENSITIVITY;
                    float deltaY = gyroX * MOVEMENT_SENSITIVITY;

                    // Apply low-magnitude deadzone threshold to eliminate micro hand-jitters when static
                    if (Math.abs(deltaX) < 0.15f) deltaX = 0f;
                    if (Math.abs(deltaY) < 0.15f) deltaY = 0f;

                    listener.onMouseMove(deltaX, deltaY);

                    // 4. Click Gesture Threshold Detection
                    // Rapid rotation to the left around Y-axis triggers left click
                    if (gyroY > GYRO_Y_CLICK_THRESHOLD && (currentTime - lastClickTime > GESTURE_DEBOUNCE_MS)) {
                        lastClickTime = currentTime;
                        listener.onClick();
                    }
                }
                lastTimestamp = event.timestamp;

                // Send debug details back to the interface
                listener.onSensorDebugUpdate(
                        "Gyro (deg/s): " + String.format("%.2f, %.2f, %.2f", gyroX, gyroY, gyroZ),
                        "Accel (m/s²): " + String.format("%.2f, %.2f, %.2f", rawAccel[0], rawAccel[1], rawAccel[2]),
                        "Orientation: " + String.format("%.2f, %.2f", orientation[2], orientation[0])
                );
                break;

            case Sensor.TYPE_ACCELEROMETER:
                // Apply 6-position Linear Calibration Equation: Corrected = (Raw - Offset) / Scale
                rawAccel[0] = (event.values[0] - accelOffset[0]) / accelScale[0];
                rawAccel[1] = (event.values[1] - accelOffset[1]) / accelScale[1];
                rawAccel[2] = (event.values[2] - accelOffset[2]) / accelScale[2];
                hasAccel = true;

                // 5. Dual-Directional Scroll Gesture Threshold Detection
                // High acceleration along Y-axis triggers scrolling. Positive = Up, Negative = Down
                float dynamicAccelY = rawAccel[1];
                if (Math.abs(dynamicAccelY) > ACCEL_Y_SCROLL_THRESHOLD && (currentTime - lastScrollTime > GESTURE_DEBOUNCE_MS)) {
                    lastScrollTime = currentTime;
                    int direction = (dynamicAccelY > 0) ? 1 : -1;
                    listener.onScroll(direction);
                }
                break;

            case Sensor.TYPE_MAGNETIC_FIELD:
                // Apply Figure-8 Shape Calibration Equation
                rawMag[0] = (event.values[0] - magOffset[0]) / magScale[0];
                rawMag[1] = (event.values[1] - magOffset[1]) / magScale[1];
                rawMag[2] = (event.values[2] - magOffset[2]) / magScale[2];
                hasMag = true;
                break;
        }

        Trace.endSection(); // AirMouse_OnSensorChanged
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // No action required here
    }
}