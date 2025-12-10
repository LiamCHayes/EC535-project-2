#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <unistd.h>
#include <stdint.h>

// Data structure definitions
typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} imu_data_t;

// Function declarations
int imu_init(int file_handle);
imu_data_t imu_read(int file_handle);

// Constants
#define ACCEL_SCALE_FACTOR 16384.0f
#define GYRO_SCALE_FACTOR 131.0f

#endif
