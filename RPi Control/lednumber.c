#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define FBDEVICE "/dev/fb0"

extern inline unsigned short makepixel(unsigned char r, unsigned char g, unsigned char b){
	return (unsigned short)(((r>>3)<<11) | ((g>>2)<<5) | (b>>3));
}
int setLed(int num){
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	unsigned short *pfb;
	int fbfd, size;
	unsigned short c = makepixel(127, 0, 127);
	unsigned short number[11][64] = {{0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, c, c, c, 0, \
									  0, c, c, c, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0}, 
									 {0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, c, c, c, 0, 0 ,0, \
									  0, 0, c, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, c, c, c, c, 0, 0},
									 {0, 0, c, c, c, c, 0, 0, \
									  0, c, c, c, c, c, c, 0, \
									  0, c, 0, 0, 0, c, c, 0, \
									  0, 0, 0, 0, c, c, 0, 0, \
									  0, 0, c, c, 0, 0, 0, 0, \
									  0, c, c, 0, 0, 0, 0, 0, \
									  0, c, c, c, c, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0},
									 {0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, 0, 0, 0, c, c, 0, \
									  0, 0, 0, c, c, c, 0, 0, \
									  0, 0, 0, c, c, c, c, 0, \
									  0, 0, 0, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0}, 
									 {0, 0, 0, 0, c, c, 0, 0, \
									  0, 0, 0, c, c, c, 0, 0, \
									  0, 0, c, 0, c, c, 0, 0, \
									  0, c, 0, 0, c, c, 0, 0, \
									  0, c, c, c, c, c, c, 0, \
									  0, c, c, c, c, c, c, 0, \
									  0, 0, 0, 0, 0, c, c, 0, \
									  0, 0, 0, 0, 0, c, c, 0},
									 {0, c, c, c, c, c, c, 0, \
									  0, c, c, 0, 0, 0, 0, 0, \
									  0, c, c, 0, 0, 0, 0, 0, \
									  0, c, c, c, c, c, 0, 0, \
									  0, 0, 0, 0, 0, c, c, 0, \
									  0, c, 0, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0}, 
									 {0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, 0, c, 0, \
									  0, c, c, 0, 0, 0, 0, 0, \
									  0, c, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0},
									 {0, c, c, c, c, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, 0, 0, 0, c, c, 0, \
									  0, 0, 0, 0, c, c, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0, \
									  0, 0, 0, c, c, 0, 0, 0},
									 {0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, c, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0},
									 {0, 0, c, c, c, c, 0, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, c, c, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, c, 0, \
									  0, 9, 0, 0, 0, c, c, 0, \
									  0, c, 0, 0, 0, c, c, 0, \
									  0, 0, c, c, c, c, 0, 0}, 
									 {0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0, \
									  0, 0, 0, 0, 0, 0, 0, 0}
									  };
	
	fbfd = open(FBDEVICE, O_RDWR);
	if(fbfd < 0){
		perror("Error : cannot open framebuffer device");
		return EXIT_FAILURE;
	}
	ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
	if(strcmp(finfo.id, "RPi-Sense FB") != 0){		//check if sensehat is correct
		printf("%s\n", "Error : RPi-Sense FB not found");
		close(fbfd);
		return EXIT_FAILURE;
	}
	if(ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0){
		perror("Error reading fixed information");
		return EXIT_FAILURE;
	}
	size = vinfo.xres * vinfo.yres * sizeof(short);
	//calculate size of buffer using screen info
	//map frame buffer into memory
	pfb = (unsigned short*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if(pfb == MAP_FAILED){
		close(fbfd);
		perror("Error mmapping the file");
		return EXIT_FAILURE;
	}
	
    memcpy(pfb, number[num], size);

/*	memset(pfb, 0, size);
	*(pfb+0+0*vinfo.xres) = makepixel(255, 0, 0);
	*(pfb+2+2*vinfo.xres) = makepixel(0, 255, 0);
	*(pfb+4+4*vinfo.xres) = makepixel(0, 0, 255);
	*(pfb+6+6*vinfo.xres) = makepixel(255, 0, 255);
	*/
	if(munmap(pfb, size) == -1){
		perror("Error un-mmapping the file");
	}
	close(fbfd);
	return EXIT_SUCCESS;
}
