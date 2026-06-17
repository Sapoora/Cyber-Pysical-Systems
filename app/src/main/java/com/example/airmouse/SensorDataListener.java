package com.example.airmouse;


public interface SensorDataListener {


    void onMouseMove(float deltaX, float deltaY);

    void onClick();

    void onScroll(int direction);

    void onSensorDebugUpdate(String gyroText, String accelText, String magnetText);
}