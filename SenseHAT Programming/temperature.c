#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <sys/ioctl.h>

static const char* I2C_DEV = "/dev/i2c-1";      //device file for I2C 
static const int I2C_SLAVE = 0x0703;           //value to set up I2C_SLAVE set up in ioctl() 

static const int HTS221_ID = 0x5F;              //senseHAT I2C-1's value

static const int CTRL_REG1 = 0x20;
static const int CTRL_REG2 = 0x21;

static const int H0_T0_OUT_L = 0x36;
static const int H0_T0_OUT_H = 0x37;
static const int H1_T0_OUT_L = 0x3A;
static const int H1_T0_OUT_H = 0x3B;
static const int H0_rH_x2 = 0x30;
static const int H1_rH_x2 = 0x31;

static const int H_T_OUT_L = 0x28;
static const int H_T_OUT_H = 0x29;
static const int T0_OUT_L = 0x3C;
static const int T0_OUT_H = 0x3D;
static const int T1_OUT_L = 0x3E;
static const int T1_OUT_H = 0x3F;
static const int T0_degC_x8 = 0x32;
static const int T1_degC_x8 = 0x33;
static const int T1_T0_MSB = 0x35;

static const int TEMP_OUT_L = 0x2A;
static const int TEMP_OUT_H = 0x2B;

//function for temp and humidity
void getTemperature(int fd, double *temperature, double *humidity);

int main() {
    int i2c_fd;
    double temperature, humidity;

    //open I2C device file
    if((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        perror("Unable to open i2c device");
        return 1;
    }
    //set up HTS221 with I2C device as slave mode
    if(ioctl(i2c_fd, I2C_SLAVE, HTS221_ID) < 0) {
        perror("Unable to configure i2c slave device");
        close(i2c_fd);
        return 1;
    }
    for(int i=0; i<10; i++) {
        //Initialize HTS221 device
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x00);
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x84);
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG2, 0x01);
        //get temp and humidity
        getTemperature(i2c_fd, &temperature, &humidity);
        //print calculated value
        printf("Temperature*(from HTS221) = %.2f˚C\n", temperature);
        printf("Humidity = %.0f%% rH\n", humidity);

        delay(1000);
    }
    //wrap finished device
    wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x00);
    close(i2c_fd);

    return 0;
}
//function to get temp and humidity
void getTemperature(int fd, double *temperature, double *humidity) {
    int result;

    do {
        delay(25);
        result = wiringPiI2CReadReg8(fd, CTRL_REG2);
    } while(result !=0);

    //read 
    unsigned char t0_out_l = wiringPiI2CReadReg8(fd, T0_OUT_L);
    unsigned char t0_out_h = wiringPiI2CReadReg8(fd, T0_OUT_H);
    unsigned char t1_out_l = wiringPiI2CReadReg8(fd, T1_OUT_L);
    unsigned char t1_out_h = wiringPiI2CReadReg8(fd, T1_OUT_H);

    unsigned char t0_degC_x8 = wiringPiI2CReadReg8(fd, T0_degC_x8);
    unsigned char t1_degC_x8 = wiringPiI2CReadReg8(fd, T1_degC_x8);
    unsigned char t1_t0_msb = wiringPiI2CReadReg8(fd, T1_T0_MSB);

    unsigned char h0_out_l = wiringPiI2CReadReg8(fd, H0_T0_OUT_L);
    unsigned char h0_out_h = wiringPiI2CReadReg8(fd, H0_T0_OUT_H);
    unsigned char h1_out_l = wiringPiI2CReadReg8(fd, H1_T0_OUT_L);
    unsigned char h1_out_h = wiringPiI2CReadReg8(fd, H1_T0_OUT_H);

    unsigned char h0_rh_x2 = wiringPiI2CReadReg8(fd, H0_rH_x2);
    unsigned char h1_rh_x2 = wiringPiI2CReadReg8(fd, H1_rH_x2);

    short s_t0_out = t0_out_h << 8 | t0_out_l;
    short s_t1_out = t1_out_h << 8 | t1_out_l;

    short s_h0_t0_out = h0_out_h << 8 | h0_out_l;
    short s_h1_t0_out = h1_out_h << 8 | h1_out_l;
    //create 16 / 10 bit values
    unsigned short s_t0_degC_x8 = (t1_t0_msb & 3) << 8 | t0_degC_x8;
    unsigned short s_t1_degC_x8 = ((t1_t0_msb & 12) >> 2 ) << 8 | t1_degC_x8;
    //calibrate temp
    double d_t0_degC = s_t0_degC_x8 / 8.0;
    double d_t1_degC = s_t1_degC_x8 / 8.0;
    //calibrate humidity
    double h0_rH = h0_rh_x2 / 2.0;
    double h1_rH = h1_rh_x2 / 2.0;
    //y = mx +c 
    double t_gradient_m = (d_t1_degC - d_t0_degC) / (s_t1_out - s_t0_out);
    double t_intercept_c = d_t1_degC - (t_gradient_m * s_t1_out);

    double h_gradient_m = (h1_rH - h0_rH) / (s_h1_t0_out - s_h0_t0_out);
    double h_intercept_c = h1_rH - (h_gradient_m * s_h1_t0_out);
    //read temp of surrounding (2 bytes)
    unsigned char t_out_l = wiringPiI2CReadReg8(fd, TEMP_OUT_L);
    unsigned char t_out_h = wiringPiI2CReadReg8(fd, TEMP_OUT_H);
    //calculate 16 bit value
    short s_t_out = t_out_h << 8 | t_out_l;

    //read surrounding humidity (2 bytes)
    unsigned char h_t_out_l = wiringPiI2CReadReg8(fd, H_T_OUT_L);
    unsigned char h_t_out_h = wiringPiI2CReadReg8(fd, H_T_OUT_H);

    //calculate 16 bit value
    short s_h_t_out = h_t_out_h << 8 | h_t_out_l;

    //calculate temp and humidity of surrounding
    *temperature = (t_gradient_m * s_t_out) + t_intercept_c;
    *humidity = (h_gradient_m * s_h_t_out) + h_intercept_c;
}
