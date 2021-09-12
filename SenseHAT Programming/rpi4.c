#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <sys/ioctl.h>

static const char* I2C_DEV = "/dev/i2c-1";
static const int I2C_SLAVE = 0x0703;

static const int LPS25H_ID = 0x5C;
static const int HTS221_ID = 0x5F;
static const int CTRL_REG1 = 0x20;
static const int CTRL_REG2 = 0x21;

static const int PRESS_OUT_XL = 0x28;
static const int PRESS_OUT_L = 0x29;
static const int PRESS_OUT_H = 0x2A;
static const int PTEMP_OUT_L = 0x28;
static const int PTEMP_OUT_H = 0x2C;

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

int kbhit(void);
void getPressure(int fd, double *temperature, double *pressue);
void getTemperature(int fd, double *temperature, double *humidity);

int main(int argc, char** argv) {
	int i=0;
	int pressure_fd, temperature_fd;
	double t_c = 0.0;
	double pressure = 0.0;
	double temperature, humidity;
	
	if((pressure_fd = open(I2C_DEV, O_RDWR)) <0) {
		perror("Unable to open i2c device");
		return 1;
	}
	
	if(ioctl(pressure_fd, I2C_SLAVE, LPS25H_ID) < 0){
	perror("Unable to configure i2c slave device");
	return 1;
}
	if((temperature_fd = open(I2C_DEV, O_RDWR)) < 0) {
	perror("Unable to open i2c device");
	return 1;
}
	if(ioctl(temperature_fd, I2C_SLAVE, HTS221_ID) < 0){
		perror("Unable to configure i2c slave device");
		close(temperature_fd);
		return 1;
	}

printf("p: Pressure, t : Temperature, q : Quit\n");

for(i=0; ;i++) {
	if(kbhit()){
		switch(getchar()) {
			case 'p':
			//Initialize LPS25H 
			wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x00);
			wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x84);
			wiringPiI2CWriteReg8(pressure_fd, CTRL_REG2, 0x01);
			
			getPressure(pressure_fd, &t_c, &pressure);
			
			printf("Temperature(from LPS25H) = %.2f C\n", t_c);
			printf("Pressure = %.0f hPa\n", pressure);
			break;
			case 't':
			//Initialize HTS221
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x00);
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x84);
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG2, 0x01);
			
			getTemperature(temperature_fd, &temperature, &humidity);
			
			printf("Temperature(from HTS221) = %.2f C\n", temperature);
			printf("Humidity = %.0f%% rH\n", humidity);
			break;
			case 'q':
			goto END;
			break;
		}
	}
	printf("%20d\t\t\r", i);
	delay(100);
}
END:
printf("Good Bye!\n");

wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x00);
close(pressure_fd);

wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x00);
close(temperature_fd);

return 0;
}

int kbhit(void) {
	struct termios oldt, newt;	//Structure for terminal
	int ch, oldf;
	
	tcgetattr(0, &oldt);		//get info from set up terminal
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);	//unset regular mode input and echo
	tcsetattr(0, TCSANOW, &newt);		//terminal set up with newt
	oldf = fcntl(0, F_GETFL, 0);
	fcntl(0, F_SETFL, oldf | O_NONBLOCK);	//set nonblock input
	
	ch = getchar();
	
	tcsetattr(0, TCSANOW, &oldt);
	fcntl(0, F_SETFL, oldf);
	
	if(ch != EOF) {
		ungetc(ch, stdin);
		return 1;
	}
	return 0;
}
void getPressure(int fd, double *temperature, double *pressure){
	int result;
	
	unsigned char temp_out_l = 0, temp_out_h =0;
	unsigned char press_out_xl =0;
	unsigned char press_out_l =0;
	unsigned char press_out_h =0;
	short temp_out =0;
	int press_out =0;
	
	//Wait until completing measurement
	do {
		delay(25);
		result = wiringPiI2CReadReg8(fd, CTRL_REG2);
	} while(result !=0);
	//Read temp value (2 bytes)
	temp_out_l = wiringPiI2CReadReg8(fd, PTEMP_OUT_L);
	temp_out_h = wiringPiI2CReadReg8(fd, PTEMP_OUT_H);
	//Read pressure value (3 bytes)
	press_out_xl = wiringPiI2CReadReg8(fd, PRESS_OUT_XL);
	press_out_l = wiringPiI2CReadReg8(fd, PRESS_OUT_L);
	press_out_h = wiringPiI2CReadReg8(fd, PRESS_OUT_H);
	
	//Synthesize value
	temp_out = temp_out_h << 8 | temp_out_l;
	press_out = press_out_h << 16 | press_out_l << 8 | press_out_xl;
	
	//Calculate 
	*temperature = 42.5 + (temp_out / 480.0);
	*pressure = press_out / 4096.0;
}

void getTemperature(int fd, double *temperature, double *humidity) {
	int result;
	
	do {
		delay(25);
		result = wiringPiI2CReadReg8(fd, CTRL_REG2);
	}while(result != 0);
	
	//Read calibrated value for temperature x-data (LSB(ADC))
	unsigned char t0_out_l = wiringPiI2CReadReg8(fd, T0_OUT_L);
	unsigned char t0_out_h = wiringPiI2CReadReg8(fd, T0_OUT_H);
	unsigned char t1_out_l = wiringPiI2CReadReg8(fd, T1_OUT_L);
	unsigned char t1_out_h = wiringPiI2CReadReg8(fd, T1_OUT_H);
	
	//Read calibrated value for temperature y-data
	unsigned char t0_degC_x8 = wiringPiI2CReadReg8(fd, T0_degC_x8);
	unsigned char t1_degC_x8 = wiringPiI2CReadReg8(fd, T1_degC_x8);
	unsigned char t1_t0_msb = wiringPiI2CReadReg8(fd, T1_T0_MSB);
	
	//Read calibrated value for humidity x-data (LSB(ADC))
	unsigned char h0_out_l = wiringPiI2CReadReg8(fd, H0_T0_OUT_L);
	unsigned char h0_out_h = wiringPiI2CReadReg8(fd, H0_T0_OUT_H);
	unsigned char h1_out_l = wiringPiI2CReadReg8(fd, H1_T0_OUT_L);
	unsigned char h1_out_h = wiringPiI2CReadReg8(fd, H1_T0_OUT_H);
	
	//Read calibrated value for humidity(% rH) y-data
	unsigned char h0_rh_x2 = wiringPiI2CReadReg8(fd, H0_rH_x2);
	unsigned char h1_rh_x2 = wiringPiI2CReadReg8(fd, H1_rH_x2);
	
	//Synthesize measured values
	short s_t0_out = t0_out_h << 8 | t0_out_l;
	short s_t1_out = t1_out_h << 8 | t1_out_l;
	
	short s_h0_t0_out = h0_out_h << 8 | h0_out_l;
	short s_h1_t0_out = h1_out_h << 8 | h1_out_l;
	
	//Create 16 / 10 bit value
	unsigned short s_t0_degC_x8 = (t1_t0_msb & 3) << 8 | t0_degC_x8;
	unsigned short s_t1_degC_x8 = ((t1_t0_msb & 12) >> 2) << 8 | t1_degC_x8;
	
	//Calculate calibrated temp y-data
	double d_t0_degC = s_t0_degC_x8 / 8.0;
	double d_t1_degC = s_t1_degC_x8 / 8.0;
	
	//Calculate calibrated humidity y-data
	double h0_rH = h0_rh_x2 / 2.0;
	double h1_rH = h1_rh_x2 / 2.0;
	
	//y = mx+c
	double t_gradient_m = (d_t1_degC - d_t0_degC) / (s_t1_out - s_t0_out);
	double t_intercept_c = d_t1_degC - (t_gradient_m * s_t1_out);
	double h_gradient_m = (h1_rH - h0_rH) / (s_h1_t0_out - s_h0_t0_out);
	double h_intercept_c = h1_rH - (h_gradient_m * s_h1_t0_out);
	
	//Read temp of surrounding (2 bytes)
	unsigned char t_out_l = wiringPiI2CReadReg8(fd, TEMP_OUT_L);
	unsigned char t_out_h = wiringPiI2CReadReg8(fd, TEMP_OUT_H);
	
	//Create 16 bit value
	short s_t_out = t_out_h << 8 | t_out_l;
	
	//Read humidty of surrounding (2 bytes)
	unsigned char h_t_out_l = wiringPiI2CReadReg8(fd, H_T_OUT_L);
	unsigned char h_t_out_h = wiringPiI2CReadReg8(fd, H_T_OUT_H);
	
	//Create 16 bit value
	short s_h_t_out = h_t_out_h << 8 | h_t_out_l;
	
	//Calculate temp/humidity of surrouding
	*temperature = (t_gradient_m * s_t_out) + t_intercept_c;
	*humidity = (h_gradient_m * s_h_t_out) + h_intercept_c;
}

	
