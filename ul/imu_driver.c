#include "imu_driver.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define I2C_BUS_FILE "/dev/i2c-2"
#define DEVICE_ADDRESS 0x68
#define REG_BANK_SEL 0x7F
#define PWR_MGMT_1 0x06
#define PWR_MGMT_2 0x07
#define WHO_AM_I_REG 0x00
#define WHO_AM_I_EXPECTED 0xEA
#define ACCEL_XOUT_H 0x2D
#define GYRO_XOUT_H 0x2D

int imu_init(int file_handle) {
    // Set the I2C slave address for the next transfers
    if (ioctl(file_handle, I2C_SLAVE, DEVICE_ADDRESS) < 0) {
        perror("Failed to acquire bus access and/or talk to slave");
        return 1;
    }

    // Set the current bank to bank 0
    uint8_t bank_select_data[2] = {REG_BANK_SEL, 0x00};
    if (write(file_handle, bank_select_data, 2) != 2) {
        perror("Failed to select User Bank 0 for reset");
        // Continue, as this might not be fatal yet
    }

    // Soft reset to default values to make sure the chip is in a clean state
    uint8_t reset_cmd[2] = {PWR_MGMT_1, 0x80};
    if (write(file_handle, reset_cmd, 2) != 2) {
        perror("Failed to perform soft reset");
        // Continue, as this might not be fatal yet
    }

    usleep(100000);

    // Wake up chip and select PLL clock
    uint8_t clock_select_cmd[2] = {PWR_MGMT_1, 0x01};
    if (write(file_handle, clock_select_cmd, 2) != 2) {
        perror("Failed to set PLL clock");
        return 1;
    }

    // Enable accelerometer and gyroscope
    uint8_t enable_sensors_cmd[2] = {PWR_MGMT_2, 0x00};
    if (write(file_handle, enable_sensors_cmd, 2) != 2) {
        perror("Failed to enable sensors");
        return 1;
    }

    usleep(10000);
    return 0;
}

imu_data_t imu_read(int file_handle) {
    imu_data_t data = {0};

    // Acceleration
    uint8_t reg_addr_accel = ACCEL_XOUT_H;
    uint8_t accel_data_buffer[6] = {0};
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset;

    msgs[0].addr = DEVICE_ADDRESS;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg_addr_accel;

    msgs[1].addr = DEVICE_ADDRESS;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 6;
    msgs[1].buf = &accel_data_buffer;

    msgset.msgs = msgs;
    msgset.nmsgs = 2;

    if (ioctl(file_handle, I2C_RDWR, &msgset) < 0) {
        perror("Failed to read acceleration data");
        close(file_handle);
        exit(1);
    }

    int16_t accel_x = (int16_t)(accel_data_buffer[0] << 8 | accel_data_buffer[1]);
    int16_t accel_y = (int16_t)(accel_data_buffer[2] << 8 | accel_data_buffer[3]);
    int16_t accel_z = (int16_t)(accel_data_buffer[4] << 8 | accel_data_buffer[5]);

    data.accel_x = (float)accel_x / ACCEL_SCALE_FACTOR;
    data.accel_y = (float)accel_y / ACCEL_SCALE_FACTOR;
    data.accel_z = (float)accel_z / ACCEL_SCALE_FACTOR;

    // Gyroscope
    uint8_t reg_addr_gyro = GYRO_XOUT_H;
    uint8_t gyro_data_buffer[6] = {0};
    struct i2c_msg msgs_gyro[2];
    struct i2c_rdwr_ioctl_data msgset_gyro;

    msgs_gyro[0].addr = DEVICE_ADDRESS;
    msgs_gyro[0].flags = 0;
    msgs_gyro[0].len = 1;
    msgs_gyro[0].buf = &reg_addr_gyro;

    msgs_gyro[1].addr = DEVICE_ADDRESS;
    msgs_gyro[1].flags = I2C_M_RD;
    msgs_gyro[1].len = 6;
    msgs_gyro[1].buf = &gyro_data_buffer;

    msgset_gyro.msgs = msgs_gyro;
    msgset_gyro.nmsgs = 2;

    if (ioctl(file_handle, I2C_RDWR, &msgset_gyro) < 0) {
        perror("Failed to read gyroscope data");
        close(file_handle);
        exit(1);
    }

    int16_t gyro_x = (int16_t)(gyro_data_buffer[0] << 8 | gyro_data_buffer[1]);
    int16_t gyro_y = (int16_t)(gyro_data_buffer[2] << 8 | gyro_data_buffer[3]);
    int16_t gyro_z = (int16_t)(gyro_data_buffer[4] << 8 | gyro_data_buffer[5]);

    data.gyro_x = (float)gyro_x / GYRO_SCALE_FACTOR;
    data.gyro_y = (float)gyro_y / GYRO_SCALE_FACTOR;
    data.gyro_z = (float)gyro_z / GYRO_SCALE_FACTOR;

    return data;
}
