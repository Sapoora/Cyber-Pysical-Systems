package com.example.airmouse;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class CalibrationActivity extends AppCompatActivity {

    private TextView stepTitle;
    private TextView stepInstruction;
    private TextView statusText;
    private ProgressBar progressBar;
    private Button actionButton;
    private Button finishButton;

    private final Handler handler = new Handler(Looper.getMainLooper());

    // calibration steps: 0 = gyro, 1 = accel, 2 = magnetometer
    private int currentStep = 0;
    private static final int TOTAL_STEPS = 3;

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

        showStep(currentStep);

        actionButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                runCurrentStep();
            }
        });

        finishButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                finish(); // go back to MainActivity
            }
        });
    }

    private void showStep(int step) {
        switch (step) {
            case 0:
                stepTitle.setText("Step 1: Gyroscope");
                stepInstruction.setText("Place the phone on a flat, still surface and press Start.");
                break;
            case 1:
                stepTitle.setText("Step 2: Accelerometer");
                stepInstruction.setText("Place the phone in 6 different orientations (each side facing down) when asked.");
                break;
            case 2:
                stepTitle.setText("Step 3: Magnetometer");
                stepInstruction.setText("Move the phone in a figure-8 pattern in the air.");
                break;
        }
        statusText.setText("Waiting to start...");
        progressBar.setProgress(0);
        actionButton.setVisibility(View.VISIBLE);
        actionButton.setText("START STEP");
    }

    private void runCurrentStep() {
        actionButton.setVisibility(View.GONE);
        statusText.setText("Calibrating...");
        simulateProgress();
    }

    // placeholder progress simulation; real sensor reading logic goes here later
    private void simulateProgress() {
        final int[] progress = {0};
        Runnable progressRunnable = new Runnable() {
            @Override
            public void run() {
                progress[0] += 10;
                progressBar.setProgress(progress[0]);
                if (progress[0] < 100) {
                    handler.postDelayed(this, 150);
                } else {
                    onStepFinished();
                }
            }
        };
        handler.post(progressRunnable);
    }

    private void onStepFinished() {
        statusText.setText("Step complete!");
        currentStep++;
        if (currentStep < TOTAL_STEPS) {
            handler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    showStep(currentStep);
                }
            }, 800);
        } else {
            statusText.setText("All steps complete!");
            finishButton.setVisibility(View.VISIBLE);
        }
    }
}