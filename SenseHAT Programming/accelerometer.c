#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <sys/ioctl.h>

static const char* I2C_DEV = "/dev/i2c-1";
static const int I2C_SLAVE = 0x0703;

static const int LSM9DS1_MAG_ID = 0x1C;
static const int LSM9DS1_ACC_ID = 0x6A;

//set up accorindg to LSM9DS1 spec
static const int CTRL_REG1_G = 0x10;
static const int CTRL_REG4 = 0x1E;
static const int CTRL_REG6_XL = 0x20;
static const int CTRL_REG7_XL = 0x21;
static const int CTRL_REG8 = 0x22;
static const int CTRL_REG9 = 0x23;

//for gyro sensor
static const int OUT_X_G = 0x18;
static const int OUT_Y_G = 0x1A;
static const int OUT_Z_G = 0x1C;

//for accelerate sensor
static const int OUT_X_XL = 0x28;
static const int OUT_Y_XL = 0x2A;
static const int OUT_Z_XL = 0x2C;

//for magneto sensor
static const int OUT_X_L_M = 0x28;
static const int OUT_X_H_M = 0x29;
static const int OUT_Y_L_M = 0x2A;
static const int OUT_Y_H_M = 0x2B;
static const int OUT_Z_L_M = 0x2C;
static const int OUT_Z_H_M = 0x2D;

static void getAccel(int fd, int *ax, int *ay, int *az);
static void getGyro(int fd, int *gx, int *gy, int *gz);
static void getMagneto(int fd, int *mx, int *my, int *mz);

int main() {
    int acc_fd, mag_fd; 

    //initialize accel/gyro sensors
    acc_fd = open(I2C_DEV, O_RDWR);
    if(ioctl(acc_fd, I2C_SLAVE, LSM9DS1_ACC_ID) < 0) {
        perror("Failed to acquire bus for accelerometer\n");
        return 1;
    }
//initialize magneto sensor
mag_fd = open(I2C_DEV, O_RDWR);
if(ioctl(mag_fd, I2C_SLAVE, LSM9DS1_MAG_ID) < 0) {
    perror("Failed to acquire bus for magnetometer\n");
    return 1;
}
wiringPiI2CWriteReg8(acc_fd, CTRL_REG6_XL, 0x60);       //119hz accelerlation 

//initialize to enable gyroscope in every angle
wiringPiI2CWriteReg8(acc_fd, CTRL_REG4, 0x38);
wiringPiI2CWriteReg8(acc_fd, CTRL_REG1_G, 0x28);        //0x28 = 14.9hz, 500 dps

wiringPiI2CWriteReg8(mag_fd, CTRL_REG6_XL, 0x48);       //set up output data's speed/power mode
wiringPiI2CWriteReg8(mag_fd, CTRL_REG7_XL, 0x00);       //base scale
wiringPiI2CWriteReg8(mag_fd, CTRL_REG8, 0x00);          //continuous transition
wiringPiI2CWriteReg8(mag_fd, CTRL_REG9, 0x08);          //high performance mode

for(int i =0; i<23; i++) {
    int ax, ay, az;
    int gx, gy, gz;
    int mx, my, mz;

    getAccel(acc_fd, &ax, &ay, &az);
    printf("Accelerator : %d, %d, %d\n", ax, ay, az);

    getGyro(acc_fd, &gx, &gy, &gz);
    printf("Gyro : %d(pitch), %d(roll), %d(yaw)\n", gx, gy, gz);

    getMagneto(mag_fd, &mx, &my, &mz);
    printf("Magnetic : %d, %d, %d\n", mx, my, mz);

    delay(400);
}
//close fd
close(acc_fd);
close(mag_fd);

return 0;
}
void getAccel(int fd, int *ax, int *ay, int *az) {
    //read values from accelerate sensor
    *ax = wiringPiI2CReadReg16(fd, OUT_X_XL);
    *ay = wiringPiI2CReadReg16(fd, OUT_Y_XL);
    *az = wiringPiI2CReadReg16(fd, OUT_Z_XL);

    //verify border
    if(*ax > 32767) *ax -= 65536;
    if(*ay > 32767) *ay -= 65536;
    if(*az > 32767) *az -= 65536;
}
void getGyro(int fd, int *gx, int *gy, int *gz) {
    //read gyro sensor value
    *gx = wiringPiI2CReadReg16(fd, OUT_X_G);
    *gy = wiringPiI2CReadReg16(fd, OUT_Y_G);
    *gz = wiringPiI2CReadReg16(fd, OUT_Z_G);
}
void getMagneto(int fd, int *mx, int *my, int *mz) {
    unsigned char out_x_l_m = wiringPiI2CReadReg8(fd, OUT_X_L_M);
    unsigned char out_x_h_m = wiringPiI2CReadReg8(fd, OUT_X_H_M);
    unsigned char out_y_l_m = wiringPiI2CReadReg8(fd, OUT_Y_L_M);
    unsigned char out_y_h_m = wiringPiI2CReadReg8(fd, OUT_Y_H_M);
    unsigned char out_z_l_m = wiringPiI2CReadReg8(fd, OUT_Z_L_M);
    unsigned char out_z_h_m = wiringPiI2CReadReg8(fd, OUT_Z_H_M);

    *mx = out_x_l_m + (out_x_h_m << 8);
    *my = out_y_l_m + (out_y_h_m << 8);
    *mz = out_z_l_m + (out_z_h_m << 8);

    //check +- of number : verify border
    if(*mx > 32767) *mx -= 65536;
    if(*my > 32767) *my -= 65536;
    if(*mz > 32767) *mz -= 65536;
}
