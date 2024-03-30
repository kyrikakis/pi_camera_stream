import time
import RTIMU
import math

SETTINGS_FILE = "RTIMULib"

s = RTIMU.Settings(SETTINGS_FILE)
imu = RTIMU.RTIMU(s)

if not imu.IMUInit():
    print("IMU Init Failed")
else:
    imu.setSlerpPower(0.02)
    imu.setGyroEnable(True)
    imu.setAccelEnable(True)
    imu.setCompassEnable(False)

    poll_interval = imu.IMUGetPollInterval()
    print("Poll Interval: %d ms" % poll_interval)

    try:
        while True:
            if imu.IMURead():
                data = imu.getIMUData()
                fusionPose = data["fusionPose"]
                roll = math.degrees(fusionPose[1])
                pitch = math.degrees(fusionPose[0]) - 90
                yaw = math.degrees(fusionPose[2])
                print(f"Roll: {roll:.2f} degrees, Pitch: {pitch:.2f} degrees, Yaw: {yaw:.2f} degrees")
                time.sleep(poll_interval/1000.0)

    except KeyboardInterrupt:
        pass
