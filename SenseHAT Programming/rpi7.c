#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/input.h>

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

static pthread_mutex_t pressure_lock;
static pthread_mutex_t temperature_lock;

static int is_run = 1;
int pressure_fd, temperature_fd;

int kbhit(void);
void getPressure(int fd, double *temperature, double *pressue);
void getTemperature(int fd, double *temperature, double *humidity);

//function for thread
void* pressureFunction(void *arg);
void* temperatureFunction(void *arg);
void* joystickFunction(void* arg);
void* webserverFunction(void* arg);
static void *clnt_connection(void *arg);
int sendData(FILE *fp, char *ct, char *filename);
void sendOk(FILE *fp);
void sendError(FILE *fp);
//??

void *pressureFunction(void *arg){
	double t_c = 0.0;
	double pressure = 0.0;
	while(is_run) {
	if(pthread_mutex_trylock(&pressure_lock) != EBUSY){
		wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x00);
		wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0X84);
		wiringPiI2CWriteReg8(pressure_fd, CTRL_REG2, 0x01);
		getPressure(pressure_fd, &t_c, &pressure);
		printf("Temperature(from LPS25H) = %.2f C\n", t_c);
		printf("Pressure = %.0f hPa\n", pressure);
		pthread_mutex_unlock(&pressure_lock);
	}
	delay(1000);
}
return NULL;
}

void *temperatureFunction(void *arg){
	double temperature, humidity;
	while(is_run){
		if(pthread_mutex_trylock(&temperature_lock) != EBUSY){
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x00);
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x84);
			wiringPiI2CWriteReg8(temperature_fd, CTRL_REG2, 0x01);
			getTemperature(temperature_fd, &temperature, &humidity);
			printf("Temperature(from HTS221) = %.2f C\n", temperature);
			printf("Humidity = %0.f%% rH\n", humidity);
			pthread_mutex_unlock(&temperature_lock);
		}
		delay(1000);
	}
	return NULL;
}	
void *joystickFunction(void *arg){
	int fd;
	struct input_event ie;
	const char* joy_dev = "/dev/input/event10";
	
	if((fd = open(joy_dev, O_RDONLY)) == 1){
		perror("opening device");
		return NULL;
	}
	while(is_run){
		read(fd, &ie, sizeof(struct input_event));
		printf("time %ld.%06ld\ttype %d\tcode %-3d\tvalue %d\n", ie.time.tv_sec, ie.time.tv_usec, ie.type, ie.code, ie.value);
		if(ie.type){
			switch(ie.code){
				case KEY_UP: printf("Up\n"); break;
				case KEY_DOWN: printf("Down\n"); break;
				case KEY_LEFT: printf("Left\n"); break;
				case KEY_RIGHT: printf("Right\n"); break;
				case KEY_ENTER: printf("Enter\n"); is_run =0; break;
				default: printf("Default\n"); break;
			}
		}
		delay(1000);
	}
	return NULL;
}

int main(int argc, char** argv){
	int i=0;
	pthread_t ptPressure, ptTemperature, ptJoystick, ptWebserver;
	pthread_mutex_init(&pressure_lock, NULL);
	pthread_mutex_init(&temperature_lock, NULL);
	
	//get port# for server when initiating a program
	if(argc !=2){
		printf("usage: %s <port>\n", argv[0]);
		return -1;
	}
	
	if((pressure_fd = open(I2C_DEV, O_RDWR)) < 0){
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
	pthread_create(&ptPressure, NULL, pressureFunction, NULL);
	pthread_create(&ptTemperature, NULL, temperatureFunction, NULL);
	pthread_create(&ptJoystick, NULL, joystickFunction, NULL);
	pthread_create(&ptWebserver, NULL, webserverFunction, (void*)(atoi(argv[1])));
	
	printf("q : Quit\n");
	for(i=0; is_run; i++){
		if(kbhit()){
			switch(getchar()){
				case 'q':
				pthread_kill(ptWebserver, SIGINT);
				pthread_cancel(ptWebserver);
				is_run = 0;
				goto END;
			};
		}
		delay(100);
	}
	END:
	printf("Good Bye!\n");

	wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x00);
	close(pressure_fd);

	wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x00);
	close(temperature_fd);

	pthread_mutex_destroy(&pressure_lock);
	pthread_mutex_destroy(&temperature_lock);

	return 0;
}
int kbhit(void){
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
	
int pressure_fd, temperature_fd;

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

void getTemperature(int fd, double *temperature, double *humidity){
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

void *webserverFunction(void* arg){
	int ssock;
	pthread_t thread;
	struct sockaddr_in servaddr, cliaddr;
	unsigned int len;
	
	int port = (int)(arg);
	
	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if(ssock == -1){
		perror("socket()");
		exit(1);
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	if(bind(ssock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
		perror("bind()");
		exit(1);
	}
	if(listen(ssock, 10) == -1){
		perror("listen()");
		exit(1);
	}
	while(is_run){
		char mesg[BUFSIZ];
		int csock;
		len = sizeof(cliaddr);
		csock = accept(ssock, (struct sockaddr*)&cliaddr, &len);
		//change network address to string
		inet_ntop(AF_INET, &cliaddr.sin_addr, mesg, BUFSIZ);
		printf("Client IP : %s:%d\n", mesg, ntohs(cliaddr.sin_port));
		
		//create a thread when client requests and process the request
		pthread_create(&thread, NULL, clnt_connection, &csock);
		//pthread_join(thread, NULL);
	}
		return 0;
	}
	void *clnt_connection(void *arg){
		//convert arg via thread into int type fd
		int csock = *((int*)arg), clnt_fd;
	FILE *clnt_read, *clnt_write;
	char reg_line[BUFSIZ], reg_buf[BUFSIZ];
	char method[BUFSIZ], type[BUFSIZ];
	char filename[BUFSIZ], *ret;
	
	//convert fd into file stream
	clnt_read = fdopen(csock, "r");
	clnt_write = fdopen(dup(csock), "w");
	
	//read one line of string and store to reg_line
	fgets(reg_line, BUFSIZ, clnt_read);
	//print reg_line string on screen
	fputs(reg_line, stdout);
	//
	ret = strtok(reg_line, "/ ");
	strcpy(method, (ret != NULL)?ret:"");
	if(strcmp(method, "POST") == 0){		//for POST method
		sendOk(clnt_write);		//send ok to client
		goto END;
	}else if(strcmp(method, "GET") != 0){		//if not GET method
		sendError(clnt_write);
		goto END;
	}
	ret = strtok(NULL, " ");		//get a path from request line
	strcpy(filename, (ret != NULL)?ret:"");
	if(filename[0] == '/'){			//if path starts with /, remove it
		for(int i=0, j=0; i< BUFSIZ; i++, j++){
			if(filename[0] == '/') j++;
			filename[i] = filename[j];
			if(filename[j] == '\0') break;
	}
}
if(!strlen(filename)) strcpy(filename, "index.html");
//process HTML code after analysis to control RPi
if(strstr(filename, "?") != NULL){
	char optLine[BUFSIZ];
	char optStr[32][BUFSIZ];
	char opt[BUFSIZ], var[BUFSIZ], *tok;
	int count =0;
	
	ret = strtok(filename, "?");
	if(ret == NULL) goto END;
	strcpy(filename, ret);
	ret = strtok(NULL, "?");
	if(ret == NULL) goto END;
	strcpy(optLine, ret);
	//analyze options
	tok = strtok(optLine, "&");
	while (tok != NULL){
		strcpy(optStr[count++], tok);
		tok = strtok(NULL, "&");
	}
	//process analyzed option
	for(int i=0; i<count; i++){
		strcpy(opt, strtok(optStr[i], "="));
		strcpy(var, strtok(NULL, "="));
		printf("%s = %s\n", opt, var);
		if(!strcmp(opt, "led") && !strcmp(var, "On")){		//turn on 8x8 mat
		}else if(!strcmp(opt, "led") && !strcmp(var, "Off")){	//turn off 8x8
		}
	}
}
do{
		fgets(reg_line, BUFSIZ, clnt_read);
		fputs(reg_line, stdout);
		strcpy(reg_buf, reg_line);
		char* type = strchr(reg_buf, ':');
	}while(strncmp(reg_line, "\r\n", 2));		//request header ends with \r\n
	
	//send file contents to client using file's name
	sendData(clnt_write, type, filename);
	
	END:
	fclose(clnt_read);		//close file's stream
	fclose(clnt_write);
	
	pthread_exit(0);
	

	return (void*)NULL;
}
extern int captureImage(int fd);

int sendData(FILE *fp, char *ct, char *filename){
	char protocol[] = "HTTP/1.1 200 OK\r\n";
	char server[] = "Server:Netscape-Enterprise/6.0\r\n";
	char cnt_type[] = "Content-Type:text/html\r\n";
	char end[] = "\r\n";
	char html[BUFSIZ];
	double temperature, humidity;
	double t_c = 0.0;
	double pressure = 0.0;
	
	wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x00);
	wiringPiI2CWriteReg8(pressure_fd, CTRL_REG1, 0x84);
	wiringPiI2CWriteReg8(pressure_fd, CTRL_REG2, 0x01);
	getPressure(pressure_fd, &t_c, &pressure);
	
	wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x00);
	wiringPiI2CWriteReg8(temperature_fd, CTRL_REG1, 0x84);
	wiringPiI2CWriteReg8(temperature_fd, CTRL_REG2, 0x01);
	getTemperature(temperature_fd, &temperature, &humidity);
	
	sprintf(html, "<html><head><meta http-equiv=\"Content-Type\" " \
				  "content=\"text/html; charset=UTF-8\" />" \
				  "<meta http-equiv-\"refresh\"content=\"5\"; url=\"control.html>" \
				  "<title>Raspberry Pi Observation Server</title></head><body><table>" \
				  "<tr><td>Temperature</td><td colspan=2>" \
				  "<input readonly name=\"temperature\"value=%.3f></td></tr>" \
				  "<tr><td>Humidity</td><td colspan=2>" \
				  "<input readonly name=\"humidity\"value=%.3f></td></tr>" \
				  "<form action=\"index.html\" method=\"GET\" "\
				  "onSubmit=\"document.reload()\"><table>" \
				  "<tr><td>8x8 LED Matrix</td><td>" \
				  "<input type=radio name\"led\" value=\"On\" checked=checked>On</td>" \
				  "<td><input type=rradio name=\"led\" value=\"Off\">Off</td>" \
				  "</tr><tr><td>Submit</td>" \
				  "<td colspan=2><input type=submit value=\"Submit\"></td></tr>" \
				  "</table></form></body></html>", temperature, humidity, pressure);
				  
		fputs(protocol, fp);
		fputs(server, fp);
		
		if(!strcmp(filename, "capture.bmp")){
			char cnt_type[] = "Content-Type:image/bmp\r\n";
			fputs(cnt_type, fp);
			fputs(end, fp);
			captureImage(fileno(fp));
			int len, fd = open(filename, O_RDONLY);
			do{
				len = read(fd, html, BUFSIZ);
				fwrite(html, len, sizeof(char), fp);
			}while(len == BUFSIZ);
			close(fd);
		}else{
		fputs(cnt_type, fp);
		fputs(end, fp);
		fputs(html, fp);
	}
		fflush(fp);
		
		return 0;
	}
	void sendOk(FILE *fp){
		//http response msg for success to client
		char protocol[] = "HTTP/1.1 200 Ok\r\n";
		char server[] = "Server: Netscape-Enterprise/6.0\r\n\r\n";
		fputs(protocol, fp);
		fputs(server, fp);
		fflush(fp);
	}
	void sendError(FILE *fp){
		char protocol[] = "HTTP/1.1 400 Bad Request\r\n";
		char server[] = "Server: Netscape-Enterprise/6.0\r\n";
		char cnt_len[] = "Content-Length:1024\r\n";
		char cnt_type[] = "Content-Type:text/html\r\n\r\n";
		
		//html content to display
		char content1[] = "<html><head><title>BAD Connection</title></head>";
		char content2[] = "<body><font size=+5>Bad Request</font></body></html>";
		
		printf("send_error\n");
		fputs(protocol, fp);
		fputs(server, fp);
		fputs(cnt_len, fp);
		fputs(cnt_type, fp);
		fputs(content1, fp);
		fputs(content2, fp);
		fflush(fp);
	}
