import math

class CoreSensorFilter:
    def __init__(self, alpha=0.98):
        self.alpha = alpha
        self.pitch = 0.0  # pitch angle (deg)
        self.roll = 0.0   # roll angle (deg)
        self.last_timestamp = None

    def update(self, accel_x, accel_y, accel_z, gyro_x, gyro_y, timestamp_ns):
        """Process raw sensor data and return filtered angles.
        `timestamp_ns` is sensor time in Android nanoseconds.
        """
        # compute dt (ns -> s)
        if self.last_timestamp is None:
            self.last_timestamp = timestamp_ns
            return self.pitch, self.roll
        
        dt = (timestamp_ns - self.last_timestamp) / 1000000000.0
        self.last_timestamp = timestamp_ns

        # compute accel-based angles (deg)
        accel_pitch = math.atan2(accel_y, math.sqrt(accel_x**2 + accel_z**2)) * (180.0 / math.pi)
        accel_roll = math.atan2(-accel_x, accel_z) * (180.0 / math.pi)
        # complementary filter: combine gyro (deg/s) and accel angles
        self.pitch = self.alpha * (self.pitch + gyro_x * dt) + (1.0 - self.alpha) * accel_pitch
        self.roll = self.alpha * (self.roll + gyro_y * dt) + (1.0 - self.alpha) * accel_roll

        return self.pitch, self.roll