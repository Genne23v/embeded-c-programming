#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include "bmpHeader.h"

#define VIDEODEV "/dev/video10"
#define FBDEV "/dev/fb0"
#define WIDTH 800
#define HEIGHT 600
#define NUMCOLOR 3


//struct for buffer to save video to be used in v4l
struct buffer{
	void* start;
	size_t length;
};

static int fd = -1;
struct buffer *buffers = NULL;
static int fbfd = -1;
static unsigned char *fbp = NULL;
static struct fb_var_screeninfo vinfo; 

static void processImage(const void *p);
static int readFrame();
static void initRead(unsigned int buffer_size);
static void initDevice();
static void mainloop();
static void uninitDevice();
void saveImage(unsigned char *inimg);

extern int captureImage(int fd){
	long screensize = 0;
	
	fd = open(VIDEODEV, O_RDWR | O_NONBLOCK, 0);
	if(fd == -1){
		perror("open() : video device");
		return EXIT_FAILURE;
	}
	fbfd = open(FBDEV, O_RDWR);
	if(fbfd == -1){
		perror("open() : framebuffer device");
		return EXIT_FAILURE;
	}
	if(ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1){
		perror("Error reading variable information");
		exit(EXIT_FAILURE);
	}
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel/8;
	fbp = (unsigned char*)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if((int)fbp == -1){
		perror("mmap() : framebuffer device to memory");
		return EXIT_FAILURE;
	}
	memset(fbp, 0, screensize);
	
	initDevice();
	mainloop();
	uninitDevice();
	
	close(fbfd);
	close(fd);
	
	return EXIT_SUCCESS;
}
static void initDevice(){
	struct v4l2_capability cap;		//study functions of video device
	struct v4l2_format fmt;
	unsigned int min;
	
	//study if v4l2 supported using v4l2_capability struct
	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
		if(errno == EINVAL) {
			perror("/dev/video0 is no V4L2 device");
			exit(EXIT_FAILURE);
		}else {
			perror("VIDIOC_QUERYCAP");
			exit(EXIT_FAILURE);
		}
	}
	//see if camera has a capture capability
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
		perror("/dev/video0 is no video capture device");
		exit(EXIT_FAILURE);
	}
	//format setting of video using v4l2_format struct
	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = WIDTH;
	fmt.fmt.pix.height = HEIGHT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	
	//format setting of video to camera device
	if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1){
		perror("VIDIOC_S_FMT");
		exit(EXIT_FAILURE);
	}
	//get min size of video
	min = fmt.fmt.pix.width * vinfo.bits_per_pixel/8;
	if(fmt.fmt.pix.bytesperline < min) fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if(fmt.fmt.pix.sizeimage < min) fmt.fmt.pix.sizeimage = min;
	
	//initialize for reading video
	initRead(fmt.fmt.pix.sizeimage);
}
static void initRead(unsigned int buffer_size){
	//allocate memory for video used by camera
	buffers = (struct buffer*)calloc(1, sizeof(*buffers));
	if(!buffers){
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
	//initialize buffer
	buffers[0].length = buffer_size;
	buffers[0].start = malloc(buffer_size);
	if(!buffers[0].start){
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}
}
#define NO_OF_LOOP 1

static void mainloop(){
	unsigned int count = NO_OF_LOOP;
	while(count-- > 0){
		for(;;){
			fd_set fds;
			struct timeval tv;
			int r;
			
			//init fd_set struct and set up fd
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			
			//set up 2 sec for timeout 
			tv.tv_sec = 2;
			tv.tv_usec = 0;
			//wait until video input
			r = select(fd +1, &fds, NULL, NULL, &tv);
			switch(r){
				case -1:		//select() errror
				if(errno != EINTR){
					perror("select()");
					exit(EXIT_FAILURE);
				}break;
				case 0:			//timeout
				perror("select timeout");
				exit(EXIT_FAILURE);
				break;
			};
			if(readFrame()) break;
		}
	}
}
static int readFrame(){
	//read a frame from camera
	if(read(fd, buffers[0].start, buffers[0].length) < 0){
		perror("read()");
		exit(EXIT_FAILURE);
	}
	//print on screen after color, space mod from frame read
	processImage(buffers[0].start);
	
	return 1;
}
//check if it's within unsigned char's area 
inline unsigned char clip(int value, int min, int max);
unsigned char clip(int value, int min, int max){
	return(value > max ? max : value < min ? min : value);
}
//YUYV to BGRA
static void processImage(const void *p){
	int j, y;
	long location =0, count =0;
	int width = WIDTH, height = HEIGHT;
	int istride = WIDTH*2;		//go to next line when out of image's width
	
	unsigned char* in = (unsigned char*)p;
	int y0, u, y1, v, colors = vinfo.bits_per_pixel/8;
	unsigned char r, g, b, a = 0xff;
	unsigned char inimg[NUMCOLOR*WIDTH*HEIGHT];
	
	for(y=0; y<height; y++, in +=istride, count =0){
		for(j=0; j<vinfo.xres*2; j+=colors){
			if(j >=width*2){		//process left space over image on current screen
				location +=colors*2;
				continue;
			}
			//break down YUYV
			y0 = in[j];
			u = in[j+1] -128;
			y1 = in[j+2];
			v = in[j+3] -128;
			//convert YUV to RGBA
			r = clip((298*y0 + 409*v +128) >> 8, 0, 255);
			g = clip((298*y0 - 100*u -208*v +128) >> 8, 0, 255);
			b = clip((298*y0 + 516*u +128) >> 8, 0, 255);
			fbp[location++] = b;
			fbp[location++] = g;
			fbp[location++] = r;
			fbp[location++] = a;
			//BMP img data
			inimg[(height-y-1)&width*NUMCOLOR+count++] = b;
			inimg[(height-y-1)&width*NUMCOLOR+count++] = g;
			inimg[(height-y-1)&width*NUMCOLOR+count++] = r;
			//YUV to RBFA: Y1
			r = clip((298*y1 + 409*v +128) >> 8, 0, 255);
			g = clip((298*y1 - 100*u -208*v +128) >> 8, 0, 255);
			b = clip((298*y1 + 516*u +128) >> 8, 0, 255);
			fbp[location++] = b;
			fbp[location++] = g;
			fbp[location++] = r;
			fbp[location++] = a;
			//BMP img data
			inimg[(height-y-1)&width*NUMCOLOR+count++] = b;
			inimg[(height-y-1)&width*NUMCOLOR+count++] = g;
			inimg[(height-y-1)&width*NUMCOLOR+count++] = r;		
			}
	}
	saveImage(inimg);
}
static void uninitDevice(){
	long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel/8;
	
	free(buffers[0].start);
	free(buffers);
	munmap(fbp, screensize);
}
void saveImage(unsigned char *inimg){
	RGBQUAD palrgb[256];
	FILE *fp;
	BITMAPFILEHEADER bmpFileHeader;
	BITMAPINFOHEADER bmpInfoHeader;
	
	//save bmp file info to BITMAPFILEHEADER struct
	memset(&bmpFileHeader, 0, sizeof(BITMAPFILEHEADER));
	bmpFileHeader.bfType = 0x4d42;		//(unsigned short)('B' | 'M' << 8
	bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpFileHeader.bfOffBits = sizeof(RGBQUAD) * 256;
	bmpFileHeader.bfSize = bmpFileHeader.bfOffBits;
	bmpFileHeader.bfSize += WIDTH*HEIGHT*NUMCOLOR;
	
	//set up bmp img info in BITMAPINFOHEADER struct
	memset(&bmpInfoHeader, 0, sizeof(BITMAPINFOHEADER));
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpInfoHeader.biWidth = WIDTH;
	bmpInfoHeader.biHeight = HEIGHT;
	bmpInfoHeader.biPlanes = 1;
	bmpInfoHeader.biBitCount = NUMCOLOR*8;
	bmpInfoHeader.SizeImage = WIDTH*HEIGHT*bmpInfoHeader.biBitCount/8;
	bmpInfoHeader.biXPelsPerMeter = 0x0B12;
	bmpInfoHeader.biYPelsPerMeter = 0xB12;
	
	if((fp = fopen("capture.bmp", "wb")) == NULL){
		fprintf(stderr, "Error : Failed to open file...\n");
		exit(EXIT_FAILURE);
	}
	fwrite((void*)&bmpFileHeader, sizeof(bmpFileHeader), 1, fp);
	fwrite((void*)&bmpInfoHeader, sizeof(bmpInfoHeader), 1, fp);
	fwrite(inimg, sizeof(unsigned char), WIDTH*HEIGHT*NUMCOLOR, fp);
	
	fclose(fp);
}
