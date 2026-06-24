package com.example.airmouse;

import android.os.Handler;
import android.os.Looper;

import java.util.Random;

// temporary simulator for UI/network testing until real sensor code is ready
public class FakeSensorEngine {

    private final SensorDataListener listener;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Random random = new Random();
    private boolean running = false;

    private int tickCount = 0;

    public FakeSensorEngine(SensorDataListener listener) {
        this.listener = listener;
    }

    public void start() {
        running = true;
        tickCount = 0;
        handler.post(tickRunnable);
    }

    public void stop() {
        running = false;
        handler.removeCallbacks(tickRunnable);
    }

    private final Runnable tickRunnable = new Runnable() {
        @Override
        public void run() {
            if (!running) return;

            tickCount++;

            float deltaX = (random.nextFloat() - 0.5f) * 10f;
            float deltaY = (random.nextFloat() - 0.5f) * 10f;
            listener.onMouseMove(deltaX, deltaY);

            listener.onSensorDebugUpdate(
                    "Gyro: [" + String.format("%.2f", random.nextFloat()) + "]",
                    "Accel: [" + String.format("%.2f", random.nextFloat()) + "]",
                    "Magnet: [" + String.format("%.2f", random.nextFloat()) + "]"
            );

            // every 60 ticks (~3 seconds) trigger a fake click
            if (tickCount % 60 == 0) {
                listener.onClick();
            }

            // every 100 ticks (~5 seconds) trigger a fake scroll
            if (tickCount % 100 == 0) {
                int direction = random.nextBoolean() ? 1 : -1;
                listener.onScroll(direction);
            }

            handler.postDelayed(this, 50);
        }
    };
}