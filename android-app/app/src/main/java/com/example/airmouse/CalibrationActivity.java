package com.example.airmouse;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

public class CalibrationActivity extends AppCompatActivity implements SensorEventListener {

    private TextView stepTitle;
    private TextView stepInstruction;
    private TextView statusText;
    private ProgressBar progressBar;
    private Button actionButton;
    private Button finishButton;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private Runnable progressRunnable;

    private SensorManager sensorManager;
    private Sensor accelSensor;
    private Sensor gyroSensor;
    private Sensor magSensor;

    private int currentStep = 0;
    private static final int TOTAL_STEPS = 3;

    // Accumulators for Gyroscope
    private final float[] gyroSum = new float[3];
    private int gyroSampleCount = 0;

    // Accumulators for Accelerometer
    private final float[] accelSum = new float[3];
    private int accelSampleCount = 0;

    // Accumulators for Magnetometer
    private float[] minMag = new float[]{Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE};
    private float[] maxMag = new float[]{Float.MIN_VALUE, Float.MIN_VALUE, Float.MIN_VALUE};

    private boolean isCollecting = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_calibration);

        stepTitle = findViewById(R.id.calibStepTitle);
        stepInstruction = findViewById(R.id.calibStepInstruction);
        statusText = findViewById(R.id.calibStatusText);
        progressBar = findViewById(R.id.calibProgressBar);
        actionButton = findViewById(R.id.calibActionButton);
        finishButton = findViewById(R.id.calibFinishButton);

        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager != null) {
            accelSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            gyroSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            magSensor = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        }

        showStep(currentStep);

        if (actionButton != null) {
            actionButton.setOnClickListener(v -> runCurrentStep());
        }
        if (finishButton != null) {
            finishButton.setOnClickListener(v -> finish());
        }
    }

    @SuppressLint("SetTextI18n")
    private void showStep(int step) {
        isCollecting = false;
        String title = "";
        String inst = "";

        switch (step) {
            case 0:
                title = "Step 1: Gyroscope Calibration";
                inst = "Leave the phone completely static on a flat surface. Do not move it.";
                break;
            case 1:
                title = "Step 2: Accelerometer Calibration";
                inst = "Keep the device flat facing upwards to capture the steady gravity vector.";
                break;
            case 2:
                title = "Step 3: Magnetometer Calibration";
                inst = "Wave the smartphone in a continuous Figure-8 pattern in the air.";
                break;
        }

        if (stepTitle != null) stepTitle.setText(title);
        if (stepInstruction != null) stepInstruction.setText(inst);
        if (statusText != null) statusText.setText("Waiting to start...");
        if (progressBar != null) progressBar.setProgress(0);
        if (actionButton != null) {
            actionButton.setVisibility(View.VISIBLE);
            actionButton.setText("START STEP");
        }
    }

    @SuppressLint("SetTextI18n")
    private void runCurrentStep() {
        if (actionButton != null) actionButton.setVisibility(View.GONE);
        if (statusText != null) statusText.setText("Calibrating... Please follow instructions.");

        // Reset buffers for the current run
        gyroSum[0] = 0; gyroSum[1] = 0; gyroSum[2] = 0;
        gyroSampleCount = 0;

        accelSum[0] = 0; accelSum[1] = 0; accelSum[2] = 0;
        accelSampleCount = 0;

        minMag = new float[]{Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE};
        maxMag = new float[]{Float.MIN_VALUE, Float.MIN_VALUE, Float.MIN_VALUE};

        isCollecting = true;

        if (sensorManager != null) {
            if (currentStep == 0 && gyroSensor != null) sensorManager.registerListener(this, gyroSensor, SensorManager.SENSOR_DELAY_GAME);
            if (currentStep == 1 && accelSensor != null) sensorManager.registerListener(this, accelSensor, SensorManager.SENSOR_DELAY_GAME);
            if (currentStep == 2 && magSensor != null) sensorManager.registerListener(this, magSensor, SensorManager.SENSOR_DELAY_GAME);
        }

        simulateProgress();
    }

    private void simulateProgress() {
        final int[] progress = {0};
        progressRunnable = new Runnable() {
            @Override
            public void run() {
                progress[0] += 1;
                if (progressBar != null) progressBar.setProgress(progress[0]);
                if (progress[0] < 100) {
                    handler.postDelayed(this, 100);
                } else {
                    isCollecting = false;
                    if (sensorManager != null) sensorManager.unregisterListener(CalibrationActivity.this);
                    calculateCalibrationParameters();
                    onStepFinished();
                }
            }
        };
        handler.post(progressRunnable);
    }

    private void calculateCalibrationParameters() {
        if (currentStep == 0 && gyroSampleCount > 0) {
            RealSensorEngine.gyroBias[0] = gyroSum[0] / gyroSampleCount;
            RealSensorEngine.gyroBias[1] = gyroSum[1] / gyroSampleCount;
            RealSensorEngine.gyroBias[2] = gyroSum[2] / gyroSampleCount;
        }
        else if (currentStep == 1 && accelSampleCount > 0) {
            // Calculate Accelerometer 1G linear parameters
            float meanX = accelSum[0] / accelSampleCount;
            float meanY = accelSum[1] / accelSampleCount;
            float meanZ = accelSum[2] / accelSampleCount;

            // When flat facing up: X and Y should be 0, Z should be +9.81 (Gravity)
            RealSensorEngine.accelOffset[0] = meanX;
            RealSensorEngine.accelOffset[1] = meanY;
            RealSensorEngine.accelOffset[2] = meanZ - 9.81f;

            // Standard scale factor defaults to stable unity
            RealSensorEngine.accelScale[0] = 1.0f;
            RealSensorEngine.accelScale[1] = 1.0f;
            RealSensorEngine.accelScale[2] = 1.0f;
        }
        else if (currentStep == 2) {
            for (int i = 0; i < 3; i++) {
                RealSensorEngine.magOffset[i] = (maxMag[i] + minMag[i]) / 2.0f;
                float scaleValue = (maxMag[i] - minMag[i]) / 2.0f;
                RealSensorEngine.magScale[i] = (scaleValue == 0) ? 1.0f : scaleValue;
            }
        }
    }

    @SuppressLint("SetTextI18n")
    private void onStepFinished() {
        if (statusText != null) statusText.setText("Step complete!");
        currentStep++;
        if (currentStep < TOTAL_STEPS) {
            handler.postDelayed(() -> showStep(currentStep), 800);
        } else {
            if (statusText != null) statusText.setText("All calibrations completed!");
            if (finishButton != null) finishButton.setVisibility(View.VISIBLE);
            Toast.makeText(this, "Calibration successfully applied!", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (!isCollecting) return;

        int type = event.sensor.getType();
        if (type == Sensor.TYPE_GYROSCOPE) {
            gyroSum[0] += event.values[0];
            gyroSum[1] += event.values[1];
            gyroSum[2] += event.values[2];
            gyroSampleCount++;
        }
        else if (type == Sensor.TYPE_ACCELEROMETER) {
            // Capturing raw stream dataset for the linear calculation step
            accelSum[0] += event.values[0];
            accelSum[1] += event.values[1];
            accelSum[2] += event.values[2];
            accelSampleCount++;
        }
        else if (type == Sensor.TYPE_MAGNETIC_FIELD) {
            for (int i = 0; i < 3; i++) {
                if (event.values[i] < minMag[i]) minMag[i] = event.values[i];
                if (event.values[i] > maxMag[i]) maxMag[i] = event.values[i];
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        isCollecting = false;
        if (sensorManager != null) {
            sensorManager.unregisterListener(this);
        }
        if (handler != null && progressRunnable != null) {
            handler.removeCallbacks(progressRunnable);
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) { }
}