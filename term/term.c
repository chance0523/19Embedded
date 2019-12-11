#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>

#include "cv.h"
#include "highgui.h"

#define FPGA_BASEADDRESS	0x88000000
#define DIP_DATA_OFFSET		0x62
#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define FBDEV_FILE "/dev/fb0"
#define CAMERA_DEVICE "/dev/camera"
#define FILE_NAME "save.jpg"

#define H_RED_ORANGE	13
#define H_ORANGE_YELLOW	27	
#define H_YELLOW_GREEN	40
#define H_GREEN_CYAN	80
#define H_CYAN_BLUE	100
#define H_BLUE_VIOLET	130
#define H_VIOLET_RED	165
#define S_LIMIT		150
#define V_LIMIT		150

unsigned char color_table[181] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,			//  0 -  9
1, 1, 1, 1, 2, 2, 2, 2, 2, 2,			// 10 - 19
2, 2, 2, 2, 2, 2, 2, 2, 4, 4,			// 20 - 29
4, 4, 4, 4, 4, 4, 4, 4, 4, 4,			// 30 - 39
4, 8, 8, 8, 8, 8, 8, 8, 8, 8,			// 40 - 49
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			// 50 - 59
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			// 60 - 69
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			// 70 - 79
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		// 80 - 89
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		// 90 - 99
32, 32, 32, 32, 32, 32, 32, 32, 32, 32,		// 100 - 109
32, 32, 32, 32, 32, 32, 32, 32, 32, 32,		// 110 - 119
32, 32, 32, 32, 32, 32, 32, 32, 32, 32,		// 120 - 129
64, 64, 64, 64, 64, 64, 64, 64, 64, 64,		// 130 - 139
64, 64, 64, 64, 64, 64, 64, 64, 64, 64,		// 140 - 149
64, 64, 64, 64, 64, 64, 64, 64, 64, 64,		// 150 - 159
64, 64, 64, 64, 64, 1, 1, 1, 1, 1,		// 160 - 169
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,			// 170 - 179
1
};
static CvMemStorage *storage = 0;
struct fb_var_screeninfo fbvar;
unsigned char *pfbmap;
unsigned short cis_rgb[320 * 240 * 2];

static struct termios initial_settings, new_settings;
static int peek_character = -1;

void init_keyboard()
{
	tcgetattr(0, &initial_settings);
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_lflag &= ~ISIG;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
}

void close_keyboard()
{
	tcsetattr(0, TCSANOW, &initial_settings);
}

int kbhit()
{
	char ch;
	int nread;

	if (peek_character != -1)
		return 1;

	new_settings.c_cc[VMIN] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
	nread = read(0, &ch, 1);
	new_settings.c_cc[VMIN] = 1;
	tcsetattr(0, TCSANOW, &new_settings);

	if (nread == 1)
	{
		peek_character = ch;
		return 1;
	}

	return 0;
}

int readch()
{
	char ch;

	if (peek_character != -1)
	{
		ch = peek_character;
		peek_character = -1;
		return ch;
	}
	read(0, &ch, 1);
	return ch;
}

int fb_display(unsigned short *rgb, int sx, int sy)
{
	int coor_x, coor_y;
	int screen_width;
	unsigned short *ptr;

	screen_width = fbvar.xres;

	for (coor_y = 0; coor_y < 240; coor_y++)
	{
		ptr = (unsigned short *)pfbmap + (screen_width * sy + sx) + (screen_width * coor_y);
		for (coor_x = 0; coor_x < 320; coor_x++)
		{
			*ptr++ = rgb[coor_x + coor_y * 320];
		}
	}
	return 0;
}

void cvIMG2RGB565(IplImage *img, unsigned short *cv_rgb, int ex, int ey)
{
	int x, y;
	unsigned char r, g, b;

	for (y = 0; y < ey; y++)
	{
		for (x = 0; x < ex; x++)
		{
			b = (img->imageData[(y * img->widthStep) + x * 3]);
			g = (img->imageData[(y * img->widthStep) + x * 3 + 1]);
			r = (img->imageData[(y * img->widthStep) + x * 3 + 2]);
			cv_rgb[y * 320 + x] = (unsigned short)RGB565(r, g, b);
		}
	}
}

void Fill_Background(unsigned short color)
{
	int x, y;

	for (y = 0; y < 480; y++)
	{
		for (x = 0; x < 800; x++)
		{
			*(unsigned short *)(pfbmap + (x) * 2 + (y) * 800 * 2) = color;
		}
	}
}

void RGB2cvIMG(IplImage *img, unsigned short *rgb, int ex, int ey)
{
	int x, y;

	for (y = 0; y < ey; y++)
	{
		for (x = 0; x < ex; x++)
		{
			(img->imageData[(y * img->widthStep) + x * 3]) = (rgb[y * ex + x] & 0x1F) << 3;               //b
			(img->imageData[(y * img->widthStep) + x * 3 + 1]) = ((rgb[y * ex + x] & 0x07E0) >> 5) << 2;  //g
			(img->imageData[(y * img->widthStep) + x * 3 + 2]) = ((rgb[y * ex + x] & 0xF800) >> 11) << 3; //r
		}
	}
}

char Short2Char(unsigned short a)
{	
	char dir;
	switch(a)
	{
		case 'a': dir = 'a'; break;
		case 'b': dir = 'b'; break;
		case 'c': dir = 'c'; break;
		case 'd': dir = 'd'; break;
		case 'e': dir = 'e'; break; 
		case 'f': dir = 'f'; break;
		case 'g': dir = 'g'; break;		
		case 'h': dir = 'h'; break;
		case 'i': dir = 'i'; break;
		case 'j': dir = 'j'; break;
		case 'k': dir = 'k'; break;
		case 'l': dir = 'l'; break; 
		case 'm': dir = 'm'; break;
		case 'n': dir = 'n'; break;		
		case 'o': dir = 'o'; break;
		case 'p': dir = 'p'; break;
		case 'q': dir = 'q'; break;
		case 'r': dir = 'r'; break; 
		case 's': dir = 's'; break;
		case 't': dir = 't'; break;
		case 'u': dir = 'u'; break;		
		case 'v': dir = 'v'; break;
		case 'w': dir = 'w'; break;
		case 'x': dir = 'x'; break;
		case 'y': dir = 'y'; break; 
		case 'z': dir = 'z'; break;
		case '0': dir = '0'; break;		
		case '1': dir = '1'; break; 
		case '2': dir = '2'; break;
		case '3': dir = '3'; break;		
		case '4': dir = '4'; break;
		case '5': dir = '5'; break;
		case '6': dir = '6'; break;
		case '7': dir = '7'; break;
		case '8': dir = '8'; break; 
		case '9': dir = '9'; break;
	}
	return dir;
}

int main(int argc, char **argv)
{
	int fbfd, fd;
	int dev, ret = 0;
	int x, y;
	int count = 0;
	int optlen = strlen("--cascade=");
	unsigned short ch = 0;
	unsigned short dipSwitchNew, dipSwitchOrig;
	unsigned short *addr_fpga, *addr_dip_data;
	char fileName[30];
	CvCapture *capture = 0;
	IplImage *image = NULL;
	IplImage *resizeImg;
	IplImage *hsvImg;
	IplImage *maskImg;
	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
	{
		perror("mem open fail\n");
		exit(1);
	}

	if ((fbfd = open(FBDEV_FILE, O_RDWR)) < 0)
	{
		printf("Failed to open: %s\n", FBDEV_FILE);
		exit(-1);
	}

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &fbvar) < 0)
	{
		perror("fbdev ioctl");
		exit(1);
	}

	if (fbvar.bits_per_pixel != 16)
	{
		fprintf(stderr, "bpp is not 16\n");
		exit(1);
	}

	pfbmap = (unsigned char *)mmap(0, fbvar.xres * fbvar.yres * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	if ((unsigned)pfbmap < 0)
	{
		perror("mmap failed...");
		exit(1);
	}
	
	// dip switch memory mapping
	/*if ((dev=open("/dev/mem",O_RDWR|O_SYNC)) < 0) 
		 {
				perror("DIPSW open fail\n");
				return -1;
	   }*/
	addr_fpga = (unsigned short *)mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, FPGA_BASEADDRESS);
	addr_dip_data = addr_fpga + DIP_DATA_OFFSET/sizeof(unsigned short);

	if(*addr_dip_data == (unsigned short)- 1)
	{
		close(fd);
		printf("mmap error \n");
		exit(1);
	}

	// lcd make blue
	storage = cvCreateMemStorage(0);
	Fill_Background(0x0011);

	//keep 변환 가능
	
	//cvResize(image, target, CV_INTER_LINEAR);

	//printf infromation

	// Initializing 
	init_keyboard();
	printf("Input the file name\n");
	/*while(count < 29 && ch != '\n')
	{
		if(kbhit())
		{ 
			ch = readch();
			fileName[count] = Short2Char(ch);
			printf("%c", fileName[count]);
			count++;
		}
	}
	printf("\n");
	fileName[count] = '\0';*/

	// name input
	//image = cvLoadImage(fileName, 1);
	image = cvLoadImage("palette_big.png", 1);
	if(image == NULL) 
	{ 
		printf("shut down\n"); 
		close_keyboard(); 
		munmap(addr_fpga, 4096); 
		close(fd); 
		return 0;
	}

	// Image matrixes define
	resizeImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
	hsvImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
	maskImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 1);
	
	// Image resizing
	cvResize(image, resizeImg, CV_INTER_LINEAR);
	cvReleaseImage(&image);

	// Image display
	cvIMG2RGB565(resizeImg, cis_rgb, 320, 240);
	fb_display(cis_rgb, 40, 120);

	// Image change RGB to HSV
	cvCvtColor(resizeImg, resizeImg, CV_BGR2HSV);

	// Make a mask Image (color table)
	for(y = 0; y < 240; y++)
		for(x = 0; x < 320; x++)
		{
			if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 1] > S_LIMIT)
			{
				if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 2] > V_LIMIT)
					maskImg->imageData[(y * maskImg->widthStep) + x] = color_table[resizeImg->imageData[(y * resizeImg->widthStep) + x * 3]];
				else 
					maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
			}
			else maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
		}
	
	// Main operation
	dipSwitchNew = ((*addr_dip_data & 0x00f0) | (*addr_dip_data & 0x000f));
	ch = 0;
	image = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
	while(ch != 'q')
	{
		if(kbhit()) ch = readch();	
		dipSwitchNew = ((*addr_dip_data & 0x00f0) | (*addr_dip_data & 0x000f));
		if(dipSwitchOrig != dipSwitchNew) 
		{
			dipSwitchOrig = dipSwitchNew;
			for(y = 0; y < 240; y++)
				for(x = 0; x < 320; x++)
				{
					if(dipSwitchOrig & maskImg->imageData[y * maskImg->widthStep + x]) 
					{
						hsvImg->imageData[y * hsvImg->widthStep + x * 3] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3];
						hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 1];
						hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 2];
					}
					else 
						hsvImg->imageData[y * hsvImg->widthStep + x * 3] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = 0;
				}
			cvCvtColor(hsvImg, image, CV_HSV2BGR);
			cvIMG2RGB565(image, cis_rgb, 320, 240);
			fb_display(cis_rgb, 435, 120);
			printf("%02X\n", dipSwitchOrig); 
		}
	}
	

	cvReleaseImage(&hsvImg);
	cvReleaseImage(&image);
	cvReleaseImage(&resizeImg);
	cvReleaseImage(&maskImg);
	close_keyboard();
	munmap(addr_fpga, 4096); 
	close(fd); 
	close(dev);
	return 0;
}
