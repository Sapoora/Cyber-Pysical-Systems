# CPS - Air mouse
A comprehensive repository dedicated to Cyber-Physical Systems (CPS) and Real-Time Embedded Systems (سیستم‌های نهفته‌ی بی‌درنگ). Features academic projects, simulations, and implementations focusing on hardware-software integration, real-time scheduling algorithms, and safety-critical system design.

# Real-Time Air Mouse (IMU-Based Human-Computer Interaction)

An advanced, real-time wireless system that transforms an Android smartphone into a high-precision spatial controller (Air Mouse) for personal computers. Developed as part of the **Real-Time Embedded Systems** curriculum at the **University of Tehran**, this system uses raw inertial/magnetic telemetry data to orchestrate screen navigation, gesture clicking, and bi-directional kinetic scrolling via low-latency custom-built networking and sensor fusion pipelines.

---

## 🚀 Key Architectural Features

* **Raw Telemetry Pipeline:** Bypasses Android’s internally filtered virtual sensors, capturing high-frequency raw hardware readings directly from the device's Accelerometer, Gyroscope, and Magnetometer.
* **Custom Native Sensor Fusion:** Implements cross-sensor calibration matrices alongside custom algorithmic implementations (such as Complementary, Kalman, or Madgwick AHRS filters) engineered directly without utilizing high-level external frameworks.
* **Low-Latency UDP Socket Transmission:** Leverages a custom thread-isolated UDP architecture utilizing JSON frame schemas to pass differential displacement indicators, event states, and contextual commands.
* **Fault-Tolerant Transmission Protocol:** Integrates a custom application-layer ACK mechanism specifically for click and scroll triggers, ensuring reliable event delivery while dropping stale translational frames to minimize lag.
* **Perfetto OS Tracing:** Profiling integrated directly into the system layer via the Perfetto trace processor API to analyze latency, CPU runtime consumption, system call blockages, and execution delays between hardware interrupts and screen updates.

---

## 🛠️ System Overview & Axis Mapping

The tracking framework assumes the user grasps the smartphone upright, perpendicular to the laptop monitor display and parallel to the plane of the keyboard layout.

| Motion Vector / Axis | Physical Input Mapping | Operating System Action |
| :--- | :--- | :--- |
| **Z-Axis Movement** | Spatial Yaw (Left/Right tilt) | Horizontal Cursor Displacement (Delta X) |
| **X-Axis Movement** | Spatial Pitch (Up/Down tilt) | Vertical Cursor Displacement (Delta Y) |
| **Y-Axis Rotation** | High-velocity impulse roll to the left | Left-Click Event Trigger |
| **Y-Axis Acceleration**| High-velocity linear displacement along the axis | Bi-directional Page Scrolling (Sign-determined) |

---

## 📂 Project Structure

```text
├── android-app/              # Native Android Studio Application Source
│   ├── app/src/main/
│   │   ├── java/com/.../        # Core telemetry sensors, filters, & network handlers
│   │   └── AndroidManifest.xml  # Cleartext traffic config & INTERNET sockets permissions
│   └── build.gradle             # Target Config (Targeting Android 10, API 29+)
│
├── windows-server/              # Computer Background Controller Client
│   ├── server.py                  # Multi-threaded UDP server parsing events & driving mouse
│
└── analytics-perfetto/          # System Trace Diagnostics Data
    ├── traces
    └── analyze_trace
└── requirements.txt  
```

---

## 🧪 Advanced Algorithmic Filtering & Mitigation

To maintain pixel-precise localization, avoid structural cursor degradation, and counteract typical non-idealities like high-frequency noise, sensory drift, and offset biases, the client applies specific signal-processing countermeasures:

### 1. Static Bias Extraction (Gyroscope)

By calculating sample means over a fixed collection of initial frames while the smartphone is held static, a localized bias vector is generated and subtracted dynamically from incoming run-time sensor arrays:

$$\omega_{\text{corrected}} = \omega_{\text{raw}} - \frac{1}{N}\sum_{i=1}^{N}\omega_{\text{static}_i}$$

### 2. Multi-Orientation Calibrations (Accelerometer)

Uses a 6-position frame sampling sequence corresponding to gravitational maximums to map linear shifts and scale discrepancies against the ideal reference of 9.81 m/s².

$$a_{\text{calibrated}} = (a_{\text{raw}} - \text{Offset}) \times \text{Scale}$$

### 3. Hard & Soft Iron Sphere Normalization (Magnetometer)

Compensates for local magnetic anomalies by tracking spatial maximums/minimums across an active 8-shaped calibration movement pattern in three dimensions:

$$\text{Offset} = \frac{\text{Max} + \text{Min}}{2}$$

$$\text{Scale} = \frac{\text{Max} - \text{Min}}{2}$$

$$B_{\text{corrected}} = \frac{B_{\text{raw}} - \text{Offset}}{\text{Scale}}$$

### 4. Adaptive Thresholding & Hysteresis

To suppress unintended gesture activation during active translational mouse tracking or physical counter-movements, the client utilizes adaptive rate-of-change thresholds paired with strict temporal lockout intervals (debounce states).

---

## ⚡ Setup & Deployment Execution

### Part 1: Initializing the Desktop Server Base

Ensure Python 3.8+ is active on the workspace machine, and configure your tracking dependencies:

```bash
cd desktop-server
pip install -r requirements.txt
python main.py

```

*Note: Make sure your target firewall exceptions allow inbound UDP listening operations on your chosen execution port (e.g., 5000 or 8080).*

### Part 2: Compiling the Client Android Application

1. Import the `android-client` codebase directory structure directly within **Android Studio**.
2. Synchronize your Gradle configurations targeting **Android 10 (API level 29)** or subsequent higher releases.
3. Input your desktop machine's dynamic LAN IP configuration address inside the UI connection field component.
4. Run the 3-phase initialization sequence inside the device UI: **Calibrate Devices** -> **Verify On-Screen Metrics Box** -> **Engage Spatial Remote Activation**.

---

## 📈 System-Level Profiling (Perfetto Tracing)

System latency tracking and resource usage analytics are performed programmatically using the system tracer engine shell:

```bash
python record_android_trace -o trace_file.perfetto-trace -t 10s sched freq idle wm gfx view

```

### Key Performance Assessments Resolved:

1. **Interrupt Latency Window:** Verifies the complete OS execution flow from the moment hardware data arrives at the register level to when the UI main loop processes the sensor event.
2. **Resource Contention Scans:** Validates resource locks and context switches between high-frequency telemetry updates and background GPU canvas rendering pipelines.
3. **Execution Profiling:** Determines the exact CPU cycle cost of our mathematical sensor fusion filtering, ensuring thread safety and preventing execution bottlenecks.

---

## 🎓 Academic Credit & Project Acknowledgments

* **Institution:** University of Tehran, Department of Electrical and Computer Engineering
* **Course:** Real-Time Embedded Systems
* **Instructors:** Dr. Mehdi Kargahi, Dr. Mohsen Shokrisaz
* **Designers:** Arslan Talaee, Arian Firoozi

```
# video link: https://drive.google.com/file/d/1fvZA2fhcY5LauYtdocS-SXMRNpQhvpK7/view?usp=sharing

```
