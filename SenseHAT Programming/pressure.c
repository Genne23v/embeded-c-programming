#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <sys/ioctl.h>

static const char* I2C_DEV = "/dev/i2c-1";      //device file for I2C 
static const int I2C_SLAVE = 0x0703;           //value to set up I2C_SLAVE set up in ioctl() 

static const int LPS25H_ID = 0x5C;              //I2C-1's value for senseHAT

//STMicroelectronics LPS25H spec document
static const int CTRL_REG1 = 0x20;              
static const int CTRL_REG2 = 0x21;

static const int PRESS_OUT_XL = 0x28;
static const int PRESS_OUT_L = 0x29;
static const int PRESS_OUT_H = 0x2A;
static const int PTEMP_OUT_L = 0x2B;
static const int PTEMP_OUT_H = 0x2C;

void getPressure(int fd, double *temperature, double *pressure);

int main() {
    int i2c_fd;
    double t_c = 0.0;
    double pressure = 0.0;

    //open I2c device 
    if((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        perror("Unable to open i2c device");
        return 1;
    }
    //Use LPS25H to set up I2C device slave mode 
    if(ioctl(i2c_fd, I2C_SLAVE, LPS25H_ID) < 0) {
        perror("Unable to configure i2c slave device");
        close(i2c_fd);
        return 1;
    }
    for(int i=0; i<10; i++) {
        //initialize LPS25H
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x00);
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x84);
        wiringPiI2CWriteReg8(i2c_fd, CTRL_REG2, 0x01);
        //get temperature, pressure values
        getPressure(i2c_fd, &t_c, &pressure);

        //print calculated value 
        printf("Temperature(from LPS25H) = %.2f˚C\n", t_c);
        printf("Pressure = %.0f hPa\n", pressure);

        delay(1000);
    }
    //wrap up device 
wiringPiI2CWriteReg8(i2c_fd, CTRL_REG1, 0x00);
close(i2c_fd);

return 0;
}
//function to get pressure and temperature
void getPressure(int fd, double *temperature, double *pressure) {
    int result;

    unsigned char temp_out_l =0, temp_out_h =0;     //to calculate temperature
    unsigned char press_out_xl =0;                  //to calculate pressure
    unsigned char press_out_l =0;
    unsigned char press_out_h =0;

//to store temperature and pressue
    short temp_out =0;
    int press_out =0;
//wait until completing measurement
do{
    delay(25);
    result = wiringPiI2CReadReg8(fd, CTRL_REG2);
}while(result !=0);

//read measured temperature (2 bytes)
temp_out_l = wiringPiI2CReadReg8(fd, PRESS_OUT_L);
temp_out_h = wiringPiI2CReadReg8(fd, PRESS_OUT_H);

//read measured pressure (3 bytes)
press_out_xl = wiringPiI2CReadReg8(fd, PRESS_OUT_XL);
press_out_l = wiringPiI2CReadReg8(fd, PRESS_OUT_L);
press_out_h = wiringPiI2CReadReg8(fd, PRESS_OUT_H);

//synthesize measured values to calculate temp, pressure (bit/shift)
temp_out = temp_out_h << 8 | temp_out_l;
press_out = press_out_h << 16 | press_out_l << 8 | press_out_xl;
//calculate
*temperature = 42.5 + (temp_out / 480.0);
*pressure = press_out / 4096.0;
}
