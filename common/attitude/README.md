# attitude

Minimal 6-axis quaternion attitude estimator for the moving FC/IMU pitch
frame. It uses BMI088 gyro plus accelerometer, applying calibration and an
axis rotation before the filter. The default BMI088 mapping is the same
`ROTATION_YAW_270` mapping used by the MicoAir PX4 board.

The estimator is a standalone, simplified port of the gravity correction and
bounded gyro-bias logic from PX4 `AttitudeEstimatorQ`. It deliberately has no
PX4 runtime dependency: no uORB, WorkQueue, parameter storage, EKF2 or GPS
declination lookup.

## Output and frame ownership

The output quaternion, roll and pitch belong to the FC/IMU frame. Its
`yaw_gyro_rad` is gyro-integrated only and is diagnostic data, not tracker
heading.

The QMC5883L is mounted on the yaw-only structure whereas the BMI088 also
rotates with pitch. They are not rigidly mounted in one sensor frame, so the
module must not fuse the QMC magnetic vector into this quaternion. The future
tracker layer owns heading: calibrated QMC5883L heading plus declination.

## API

`attitude_update_raw()` accepts BMI088 register words and applies the sensor
parameters. `attitude_update()` accepts already calibrated, rotated SI-unit
samples. Both update the same quaternion state. `tracker_hw attitude [count]`
is the bench check; it reads BMI088 accel and gyro, keeps PWM off, and prints
roll/pitch every 20 accepted samples.

The default gains are deliberately conservative. Final gain, static bias and
axis orientation must be confirmed by level, pitch-up/down and roll tests
before automatic tracker control is enabled.
