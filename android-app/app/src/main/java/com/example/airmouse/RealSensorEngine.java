package com.example.airmouse;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Trace;

import java.util.Locale;

public class RealSensorEngine implements SensorEventListener {

    private final SensorDataListener listener;
    private final SensorManager sensorManager;

    private Sensor accelerometer;
    private Sensor gyroscope;
    private Sensor magnetometer;

    private boolean running = false;

    // --- Calibration Parameters (Removed final to support clean calibration overrides) ---
    public static float[] gyroBias = new float[3];
    public static float[] accelOffset = new float[]{0f, 0f, 0f};
    public static float[] accelScale = new float[]{1f, 1f, 1f};
    public static float[] magOffset = new float[]{0f, 0f, 0f};
    public static float[] magScale = new float[]{1f, 1f, 1f};

    // --- Complementary Filter & Orientation State ---
    private final float[] orientation = new float[3]; // [0]=Pitch, [1]=Roll, [2]=Azimuth
    private long lastTimestamp = 0;
    private static final float FILTER_ALPHA = 0.95f;

    // --- Thresholds & Gesture Control ---
    private static final float MOVEMENT_SENSITIVITY = 15.0f;
    private static final float GYRO_Y_CLICK_THRESHOLD = 4.5f;
    private static final float ACCEL_Y_SCROLL_THRESHOLD = 6.0f;

    private long lastClickTime = 0;
    private long lastScrollTime = 0;
    private static final long GESTURE_DEBOUNCE_MS = 400;

    // Throttling mechanism for UI Debug updates to prevent rendering starvation
    private long lastDebugUpdateTime = 0;
    private static final long DEBUG_UPDATE_INTERVAL_MS = 100;

    // Pre-allocated arrays to avoid GC pressure inside high-frequency sensor loop
    private final float[] rawAccel = new float[3];
    private final float[] rawMag = new float[3];
    private final float[] rotationMatrixR = new float[9];
    private final float[] inclinationMatrixI = new float[9];
    private final float[] actualOrientation = new float[3];

    private boolean hasAccel = false;
    private boolean hasMag = false;

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
        lastDebugUpdateTime = 0;

        if (sensorManager != null) {
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
        hasAccel = false;
        hasMag = false;
    }

    @SuppressLint("SetTextI18n")
    @Override
    public void onSensorChanged(SensorEvent event) {
        if (!running) return;

        Trace.beginSection("AirMouse_OnSensorChanged");
        long currentTime = System.currentTimeMillis();

        switch (event.sensor.getType()) {
            case Sensor.TYPE_GYROSCOPE:
                // 1. Apply Gyro Calibration static offsets
                float gyroX = event.values[0] - gyroBias[0];
                float gyroY = event.values[1] - gyroBias[1];
                float gyroZ = event.values[2] - gyroBias[2];

                if (lastTimestamp != 0) {
                    final float dT = (event.timestamp - lastTimestamp) * 1.0e-9f;

                    Trace.beginSection("AirMouse_ComplementaryFilter");

                    // Gyro short-term Integration: [0]=Pitch(X), [1]=Roll(Y), [2]=Azimuth(Z)
                    orientation[0] += gyroX * dT;
                    orientation[1] += gyroY * dT;
                    orientation[2] += gyroZ * dT;

                    if (hasAccel && hasMag) {
                        if (SensorManager.getRotationMatrix(rotationMatrixR, inclinationMatrixI, rawAccel, rawMag)) {
                            SensorManager.getOrientation(rotationMatrixR, actualOrientation);

                            // Absolute reference fusion: [0]=Azimuth, [1]=Pitch, [2]=Roll
                            orientation[0] = FILTER_ALPHA * orientation[0] + (1.0f - FILTER_ALPHA) * actualOrientation[1]; // Pitch
                            orientation[1] = FILTER_ALPHA * orientation[1] + (1.0f - FILTER_ALPHA) * actualOrientation[2]; // Roll
                            orientation[2] = FILTER_ALPHA * orientation[2] + (1.0f - FILTER_ALPHA) * actualOrientation[0]; // Azimuth
                        }
                    }
                    Trace.endSection(); // AirMouse_ComplementaryFilter

                    // 2. Differential Motion Extraction
                    float deltaX = -gyroZ * MOVEMENT_SENSITIVITY;
                    float deltaY = -gyroX * MOVEMENT_SENSITIVITY;

                    // Low-magnitude deadzone threshold to eliminate hand trembles
                    if (Math.abs(deltaX) < 0.15f) deltaX = 0f;
                    if (Math.abs(deltaY) < 0.15f) deltaY = 0f;

                    if (listener != null) {
                        listener.onMouseMove(deltaX, deltaY);
                    }

                    // 3. Click Gesture Detection (Rapid Pitch-Y acceleration debounce)
                    if (gyroY > GYRO_Y_CLICK_THRESHOLD && (currentTime - lastClickTime > GESTURE_DEBOUNCE_MS)) {
                        lastClickTime = currentTime;
                        if (listener != null) {
                            listener.onClick();
                        }
                    }
                }
                lastTimestamp = event.timestamp;

                // UI update throttling to protect main UI thread from freezing
                if (currentTime - lastDebugUpdateTime > DEBUG_UPDATE_INTERVAL_MS) {
                    lastDebugUpdateTime = currentTime;
                    if (listener != null) {
                        listener.onSensorDebugUpdate(
                                String.format(Locale.US, "Gyro (deg/s): %.2f, %.2f, %.2f", gyroX, gyroY, gyroZ),
                                String.format(Locale.US, "Accel (m/s²): %.2f, %.2f, %.2f", rawAccel[0], rawAccel[1], rawAccel[2]),
                                String.format(Locale.US, "Orientation: %.2f, %.2f", orientation[2], orientation[0])
                        );
                    }
                }
                break;

            case Sensor.TYPE_ACCELEROMETER:
                // Apply 6-position Linear parameters properly
                rawAccel[0] = (event.values[0] - accelOffset[0]) / accelScale[0];
                rawAccel[1] = (event.values[1] - accelOffset[1]) / accelScale[1];
                rawAccel[2] = (event.values[2] - accelOffset[2]) / accelScale[2];
                hasAccel = true;

                // 4. Scroll Gesture Detection
                float dynamicAccelY = rawAccel[1];
                if (Math.abs(dynamicAccelY) > ACCEL_Y_SCROLL_THRESHOLD && (currentTime - lastScrollTime > GESTURE_DEBOUNCE_MS)) {
                    lastScrollTime = currentTime;
                    int direction = (dynamicAccelY > 0) ? 1 : -1;
                    if (listener != null) {
                        listener.onScroll(direction);
                    }
                }
                break;

            case Sensor.TYPE_MAGNETIC_FIELD:
                // Apply Figure-8 Calibration matrix to raw stream
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
        // No action required
    }
}