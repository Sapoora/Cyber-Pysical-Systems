package com.example.airmouse;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity implements SensorDataListener {

    private View cursorSquare;
    private RelativeLayout mouseArea;
    private TextView clickStatusText;
    private TextView sensorDebugText;
    private EditText ipInput;
    private Button startButton;

    private RealSensorEngine sensorEngine;
    private UdpSender udpSender;

    private float cursorX = 0f;
    private float cursorY = 0f;

    private boolean isRunning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        cursorSquare = findViewById(R.id.cursorSquare);
        mouseArea = findViewById(R.id.mouseArea);
        clickStatusText = findViewById(R.id.clickStatusText);
        sensorDebugText = findViewById(R.id.sensorDebugText);
        ipInput = findViewById(R.id.ipInput);
        startButton = findViewById(R.id.startButton);
        Button calibrateButton = findViewById(R.id.calibrateButton);

        sensorEngine = new RealSensorEngine(this, this);

        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleMouse();
            }
        });

        if (calibrateButton != null) {
            calibrateButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    Intent intent = new Intent(MainActivity.this, CalibrationActivity.class);
                    startActivity(intent);
                }
            });
        }
    }

    private void toggleMouse() {
        if (!isRunning) {
            String ip = ipInput.getText().toString().trim();
            if (ip.isEmpty()) {
                Toast.makeText(this, "Enter laptop IP first", Toast.LENGTH_SHORT).show();
                return;
            }

            udpSender = new UdpSender(ip);
            // Properly binding to UdpSender ConnectionListener callback structure
            udpSender.setListener(new UdpSender.ConnectionListener() {
                @Override
                public void onError(String message) {
                    Toast.makeText(MainActivity.this, message, Toast.LENGTH_SHORT).show();
                }
            });
            udpSender.start();

            isRunning = true;
            sensorEngine.start();
            startButton.setText("STOP MOUSE");
        } else {
            isRunning = false;
            sensorEngine.stop();
            if (udpSender != null) udpSender.stop();
            startButton.setText("START MOUSE");
        }
    }

    @Override
    public void onMouseMove(float deltaX, float deltaY) {
        int areaWidth = mouseArea.getWidth();
        int areaHeight = mouseArea.getHeight();
        int squareSize = cursorSquare.getWidth();

        cursorX += deltaX;
        cursorY += deltaY;

        float maxX = (areaWidth - squareSize) / 2f;
        float maxY = (areaHeight - squareSize) / 2f;

        if (cursorX > maxX) cursorX = maxX;
        if (cursorX < -maxX) cursorX = -maxX;
        if (cursorY > maxY) cursorY = maxY;
        if (cursorY < -maxY) cursorY = -maxY;

        cursorSquare.setTranslationX(cursorX);
        cursorSquare.setTranslationY(cursorY);

        if (udpSender != null) {
            udpSender.sendMove(deltaX, deltaY);
        }
    }

    @Override
    public void onClick() {
        clickStatusText.setText("CLICK!");
        if (udpSender != null) {
            udpSender.sendClick();
        }
    }

    @Override
    public void onScroll(int direction) {
        clickStatusText.setText(direction > 0 ? "SCROLL UP" : "SCROLL DOWN");
        if (udpSender != null) {
            udpSender.sendScroll(direction);
        }
    }

    @Override
    public void onSensorDebugUpdate(String gyroText, String accelText, String magnetText) {
        sensorDebugText.setText(gyroText + "\n" + accelText + "\n" + magnetText);
    }
}