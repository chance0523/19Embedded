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
#define DIP_DATA_READ_12	0x60
#define DIP_DATA_OFFSET		0x62
#define KEYPAD_DATA_OFFSET	0x70
#define SEGMENT_DATA_OFFSET	0x30
#define DOTMATRIX_DATA_OFFSET	0x40
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

// for image
unsigned char color_table[181] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,			//   0 -   9	
1, 1, 1, 1, 2, 2, 2, 2, 2, 2,			//  10 -  19
2, 2, 2, 2, 2, 2, 2, 2, 4, 4,			//  20 -  29
4, 4, 4, 4, 4, 4, 4, 4, 4, 4,			//  30 -  39
4, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  40 -  49
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  50 -  59
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  60 -  69
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  70 -  79
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		//  80 -  89
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		//  90 -  99
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

//for camera
unsigned char color_table_camera[181] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,			//   0 -   9 
1, 1, 1, 1, 1, 2, 2, 2, 2, 2,			//  10 -  19
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,			//  20 -  29
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,			//  30 -  39
2, 2, 2, 2, 2, 4, 4, 4, 4, 4,			//  40 -  49
4, 4, 4, 4, 4, 4, 4, 4, 4, 4,			//  50 -  59
4, 4, 4, 4, 4, 4, 4, 4, 4, 4,			//  60 -  69
4, 4, 4, 4, 4, 8, 8, 8, 8, 8,			//  70 -  79
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  80 -  89
8, 8, 8, 8, 8, 8, 8, 8, 8, 8,			//  90 -  99
8, 8, 8, 8, 8, 16, 16, 16, 16, 16,		// 100 - 109
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		// 110 - 119
16, 16, 16, 16, 16, 16, 16, 16, 16, 16,		// 120 - 129
16, 16, 16, 16, 16, 32, 32, 32, 32, 32,		// 130 - 139
32, 32, 32, 32, 32, 32, 32, 32, 32, 32,		// 140 - 149
32, 32, 32, 32, 32, 32, 32, 32, 32, 32,		// 150 - 159
32, 32, 32, 32, 32, 1, 1, 1, 1, 1,		// 160 - 169
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,			// 170 - 179
1						// 180
};

static CvMemStorage *storage = 0;
struct fb_var_screeninfo fbvar;
unsigned char *pfbmap;
unsigned short *addr_keypad_cols, *addr_keypad_rows;
unsigned short *addr_segment_select, *addr_segment_display;
unsigned short *addr_dotmatrix_cols, *addr_dotmatrix_rows;
unsigned short cis_rgb[320 * 240 * 2];
unsigned short cis_rgb_big[640 * 480 * 2];
unsigned short cis_rgb_full[800 * 480 * 2];
unsigned short cis_rgb_back[800 * 480 * 2];

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

int fb_display_camera(unsigned short *rgb)
{
	int coor_x, coor_y;
	int screen_width;
	unsigned short *ptr;

	screen_width = fbvar.xres;

	for (coor_y = 0; coor_y < 480; coor_y++)
	{
		ptr = (unsigned short *)pfbmap + 80 + (screen_width * coor_y);
		for (coor_x = 0; coor_x < 640; coor_x++)
		{
			*ptr++ = rgb[coor_x + coor_y * 640];
		}
	}
	return 0;
}

int fb_display_fullscreen(unsigned short *rgb)
{
	int coor_x, coor_y;
	int screen_width;
	unsigned short *ptr;

	screen_width = fbvar.xres;

	for (coor_y = 0; coor_y < 480; coor_y++)
	{
		ptr = (unsigned short *)pfbmap + (screen_width * coor_y);
		for (coor_x = 0; coor_x < 800; coor_x++)
		{
			*ptr++ = rgb[coor_x + coor_y * 800];
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

void cvIMG2RGB_fullscreen(IplImage *img, unsigned short *cv_rgb, int ex, int ey)
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
			cv_rgb[y * ex + x] = (unsigned short)RGB565(r, g, b);
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

int ChangeVal(unsigned short a, unsigned short b)
{
	int val, i, j;
	i = 0;
	if(a == 128 || b == 128) return 0;
	if(a == 0 || b == 0) return 0;
	while(a % 2 != 1)
	{
		a = a >> 1;
		i++;
	} 
	j = 0;
	while(b % 2 != 1)
	{
		b = b >> 1;
		j++;
	}
	val = j - i;
	if(val < 0) val += 6;
	return val * 30;
}

unsigned short Getsegmentcode (short x) 
{
	unsigned short code;
 	switch (x) 
 		{
			case 0x0 : code = 0xfc; break;
			case 0x1 : code = 0x60; break; 
			case 0x2 : code = 0xda; break; 
			case 0x3 : code = 0xf2; break; 
			case 0x4 : code = 0x66; break; 
			case 0x5 : code = 0xb6; break; 
			case 0x6 : code = 0xbe; break; 
			case 0x7 : code = 0xe4; break; 
			case 0x8 : code = 0xfe; break; 
			case 0x9 : code = 0xf6; break;
			case 0xa : code = 0xfa; break;
			case 0xb : code = 0x3e; break;
			case 0xc : code = 0x1a; break;
			case 0xd : code = 0x7a; break;
			case 0xe : code = 0x9e; break;
			case 0xf : code = 0x8e; break;
			default : code = 0; break;
		
		}
	return code;
 }  

void display_segment(unsigned short sVal, unsigned short vVal)
{
	unsigned short Num[3];
	Num[0] = sVal % 10;
	sVal /= 10;
	Num[1] = sVal % 10;
	sVal /= 10;
	Num[2] = sVal;
	*addr_segment_display = Getsegmentcode(Num[2]);
	*addr_segment_select = 0x01;
	usleep(1000);
	*addr_segment_display = Getsegmentcode(Num[1]);	
	*addr_segment_select = 0x02;
	usleep(1000);
	*addr_segment_display = Getsegmentcode(Num[0]);
	*addr_segment_select = 0x04;
	usleep(1000);

	Num[0] = vVal % 10;
	vVal /= 10;
	Num[1] = vVal % 10;
	vVal /= 10;
	Num[2] = vVal;
	*addr_segment_display = Getsegmentcode(Num[2]);
	*addr_segment_select = 0x08;
	usleep(1000);
	*addr_segment_display = Getsegmentcode(Num[1]);
	*addr_segment_select = 0x10;
	usleep(1000);
	*addr_segment_display = Getsegmentcode(Num[0]);
	*addr_segment_select = 0x20;
	usleep(1000);
}

int keypadhit()
{
	unsigned short col, row, val;
	col = row = 0;
	val = *addr_keypad_cols & 0x0f;
	while(val != 0)
	{
		if(val & 0x01) col++;
		val = val >> 1;
	}
	val = *addr_keypad_rows & 0x0f;
	while(val != 0)
	{
		if(val & 1) row++;
		val = val >> 1;
	}
	if(col == 1 && row == 1) return 1;
	else return 0;
}

void keypadValChange(int *sVal, int *vVal)
{
	unsigned short caseSel;
	caseSel = (*addr_keypad_rows << 4) | *addr_keypad_cols;
	switch (caseSel)
	{
		case 0x12 : if(*sVal < 156)	 *sVal += 100; break;
		case 0x14 :	if(*sVal < 246)	 *sVal += 10; break;
		case 0x18 :	if(*sVal < 255)	 *sVal += 1; break;
		case 0x22 : if(*sVal >= 100) *sVal -= 100; break;
		case 0x24 :	if(*sVal >= 10)	 *sVal -= 10; break;
		case 0x28 :	if(*sVal >= 1)	 *sVal -= 1; break;
		case 0x42 : if(*vVal < 156)	 *vVal += 100; break;
		case 0x44 :	if(*vVal < 246)	 *vVal += 10; break;
		case 0x48 :	if(*vVal < 255)	 *vVal += 1; break;
		case 0x82 : if(*vVal >= 100) *vVal -= 100; break;
		case 0x84 :	if(*vVal >= 10)	 *vVal -= 10; break;
		case 0x88 :	if(*vVal >= 1)	 *vVal -= 1; break;
	}
}

int main(int argc, char **argv)
{
	int fbfd, fd;
	int dev, ret = 0, brightness, saturation;
	int x, y;
	int count, mode;
	int optlen = strlen("--cascade=");
	char keypad_flag;
	unsigned short ch = 0;
	unsigned short dipSwitchNew[2], dipSwitchOrig[2];
	unsigned short *addr_fpga, *addr_dip_data, *addr_dip_select;
	unsigned short keypadVal;
	unsigned short origColor, newColor;
	int colorChangeVal; 
	char fileName[30];
	CvCapture *capture = 0;
	IplImage *image = NULL;
	IplImage *cameraImg;
	IplImage *resizeImg;
	IplImage *hsvImg;
	IplImage *maskImg;
	IplImage *loadingImg = NULL;
	IplImage *backgroundImg = NULL;
	IplImage *printImg;
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
	
	// camera open / fail check
	dev = open(CAMERA_DEVICE, O_RDWR);
	if(dev<0)
	{
		printf("Error: cannot open %s.\n", CAMERA_DEVICE);
		exit(1);
	}
	storage = cvCreateMemStorage(0);

	// dip switch memory mapping
	addr_fpga = (unsigned short *)mmap(NULL, 4096, PROT_WRITE, MAP_SHARED, fd, FPGA_BASEADDRESS);
	addr_dip_data = addr_fpga + DIP_DATA_OFFSET/sizeof(unsigned short);
	addr_dip_select = addr_fpga + DIP_DATA_READ_12/sizeof(unsigned short);
	if(*addr_dip_select == (unsigned short) - 1)
	{
		close(fd);
		printf("mmap error dip select/n");
		exit(1);
	}

	if(*addr_dip_data == (unsigned short)- 1)
	{
		close(fd);
		printf("mmap error dip data\n");
		exit(1);
	}
	
	// keypad memory mapping
	addr_keypad_cols = addr_fpga + KEYPAD_DATA_OFFSET/sizeof(unsigned short);
	addr_keypad_rows = addr_fpga + (KEYPAD_DATA_OFFSET + 2)/sizeof(unsigned short);
	if(*addr_keypad_cols == (unsigned short)-1 || *addr_keypad_rows == (unsigned short)-1)
	{
		close(fd);
		printf("mmap error keypad\n");
		exit(-1);
	}

	// segment memory mapping
	addr_segment_select = addr_fpga + SEGMENT_DATA_OFFSET/sizeof(unsigned short);
	addr_segment_display = addr_fpga + (SEGMENT_DATA_OFFSET + 2)/sizeof(unsigned short);
	if(*addr_segment_select == (unsigned short)-1 || *addr_segment_display == (unsigned short)-1)
	{
		close(fd);
		printf("mmap error segment\n");
		exit(-1);
	}

	// image loading
	loadingImg = cvLoadImage("loading.bmp", 1);
	if(loadingImg == NULL)
	{
		printf("There is no such loading Image file. Please check again\n");
	}
	printImg = cvCreateImage(cvSize(800, 480), IPL_DEPTH_8U, 3);

	for(brightness = 0; brightness < 11; brightness++)
	{
		for(y = 0; y < 480; y++)
			for(x = 0; x < 800; x++)
			{
				printImg->imageData[(y * printImg->widthStep) + x * 3] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3] * (0.1 * brightness);
				printImg->imageData[(y * printImg->widthStep) + x * 3 + 1] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3 + 1] * (0.1 * brightness);
				printImg->imageData[(y * printImg->widthStep) + x * 3 + 2] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3 + 2] * (0.1 * brightness);
			}
		cvIMG2RGB_fullscreen(printImg, cis_rgb_full, 800, 480);
		fb_display_fullscreen(cis_rgb_full);
		usleep(70000);
	}	
	sleep(2);
	cvReleaseImage(&printImg);
	cvReleaseImage(&loadingImg);

	// menu image loading
	init_keyboard();
	loadingImg = cvLoadImage("menu.bmp", 1);
	if(loadingImg == NULL)
	{
		printf("There is no such loading Image file. Please check again\n");
	}
	cvIMG2RGB_fullscreen(loadingImg, cis_rgb_full, 800, 480);
	cvReleaseImage(&loadingImg);
	while(1)
	{
		// choose mode
		fb_display_fullscreen(cis_rgb_full);
		printf(" --------------------------------------\n"); 
		printf("               Mode Select \n"); 
		printf(" --------------------------------------\n"); 
		printf(" [c] camera processing\n");
		printf(" [i] image processing\n");
		printf(" [q] exit\n");
		printf(" --------------------------------------\n\n");
		while(1) 
		{
			if(kbhit())
			{
				ch = readch();
				if(ch == 'c') { mode = 0; break; } // camera mode
				else if(ch == 'i') { mode = 1; break; } // image mode
				
				// 종료 시퀀스
				else if(ch == 'q') 
				{ 
					printf("Good bye\n\n"); 
					loadingImg = cvLoadImage("goodbye.bmp", 1);
					if(loadingImg == NULL)
					{
						printf("There is no such loading Image file. Please check again\n");
					}
					printImg = cvCreateImage(cvSize(800, 480), IPL_DEPTH_8U, 3);

					for(brightness = 10; brightness >= 0; brightness--)
					{
						for(y = 0; y < 480; y++)
							for(x = 0; x < 800; x++)
							{
								printImg->imageData[(y * printImg->widthStep) + x * 3] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3] * (0.1 * brightness);
								printImg->imageData[(y * printImg->widthStep) + x * 3 + 1] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3 + 1] * (0.1 * brightness);
								printImg->imageData[(y * printImg->widthStep) + x * 3 + 2] = loadingImg->imageData[(y * loadingImg->widthStep) + x * 3 + 2] * (0.1 * brightness);
							}
					cvIMG2RGB_fullscreen(printImg, cis_rgb_full, 800, 480);
					fb_display_fullscreen(cis_rgb_full);
					usleep(200000);
					}
					cvReleaseImage(&printImg);
					cvReleaseImage(&loadingImg);	
					close_keyboard(); munmap(addr_fpga, 4096); close(fd); Fill_Background(0x0000); return 0; 
				}
				
				// 잘못된 키 입력
				else printf("Wrong Character. Type again!\n");
			}
		}
	
		// camera mode 진입
		if(mode == 0)
		{
			// Initializing
			printf("\n");
			printf(" --------------------------------------\n");
			printf(" --------------------------------------\n"); 
			printf("          entering camera mode \n");
			printf(" --------------------------------------\n");
			printf(" --------------------------------------\n\n");
			
			backgroundImg = cvLoadImage("movie_isolation.bmp", 1);
			if(backgroundImg == NULL)
			{
				printf("There is no such loading Image file. Please check again\n");
			}
			cvIMG2RGB_fullscreen(backgroundImg, cis_rgb_back, 800, 480);
			fb_display_fullscreen(cis_rgb_back);
			cvReleaseImage(&backgroundImg);

			// image setting
			cameraImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
			hsvImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
			maskImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 1);
			resizeImg = cvCreateImage(cvSize(640, 480), IPL_DEPTH_8U, 3);

			// main operation
			ch = 0;
			saturation = S_LIMIT;
			brightness = V_LIMIT;			
			keypad_flag = 0;
			while(ch != 'q')
			{
				if(kbhit()) ch = readch();
				colorChangeVal = 0;

				*addr_dip_select = 1;
				dipSwitchNew[0] = *addr_dip_data;
				*addr_dip_select = 0;
				dipSwitchNew[1] = *addr_dip_data;
				
				// keypad and 7 segment display mode flag(1, 1)
				if(dipSwitchNew[1] >= 128 && dipSwitchNew[0] >= 128)
				{
					if(keypadhit()) 
					{
						if(keypad_flag < 3) keypad_flag++;
						if(keypad_flag == 1) keypadValChange(&saturation, &brightness);
					}
					else keypad_flag = 0;
					display_segment(saturation, brightness);			
				}
				
				// only color isolation flag(0, 1)
				else if(dipSwitchNew[1] >= 128)
				{
					*addr_segment_select = 0x00;
					
					// camera read & img process
					write(dev, NULL, 1);
					read(dev, cis_rgb, 320*240*2);
					RGB2cvIMG(cameraImg, cis_rgb, 320, 240);

					origColor = dipSwitchNew[0];
					newColor = dipSwitchNew[1];
					origColor = origColor - (origColor & (origColor - 1));
					newColor = newColor - (newColor & (newColor - 1));
					cvCvtColor(cameraImg, hsvImg, CV_BGR2HSV);
					colorChangeVal = ChangeVal(origColor, newColor);
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)
						{
							if (hsvImg->imageData[(y * hsvImg->widthStep) + x * 3 + 1] >= saturation)
							{
								if (hsvImg->imageData[(y * hsvImg->widthStep) + x * 3 + 2] >= brightness)
									maskImg->imageData[(y * maskImg->widthStep) + x] = color_table_camera[hsvImg->imageData[(y * hsvImg->widthStep) + x * 3]];
								else 
									maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
							}
							else maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
						}
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)
						{
							if(origColor & maskImg->imageData[y * maskImg->widthStep + x]) 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = (unsigned short)(((int)hsvImg->imageData[y * hsvImg->widthStep + x * 3] + colorChangeVal) % 180);
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2];
							}
							else 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = 0;
							}
						}
					cvCvtColor(hsvImg, cameraImg, CV_HSV2BGR);
					cvResize(cameraImg, resizeImg, CV_INTER_NN);
					cvIMG2RGB_fullscreen(resizeImg, cis_rgb_big, 640, 480);
					fb_display_camera(cis_rgb_big);
				}
				
				// normal camera + color isolation flag(1, 0)
				else if(dipSwitchNew[0] >= 128)
				{
					*addr_segment_select = 0x00;
					// camera read & img process
					write(dev, NULL, 1);
					read(dev, cis_rgb, 320*240*2);
					RGB2cvIMG(cameraImg, cis_rgb, 320, 240);

					origColor = dipSwitchNew[0];
					newColor = dipSwitchNew[1];
					origColor = origColor - (origColor & (origColor - 1));
					newColor = newColor - (newColor & (newColor - 1));
					cvCvtColor(cameraImg, hsvImg, CV_BGR2HSV);
					colorChangeVal = ChangeVal(origColor, newColor);
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)
						{
							if (hsvImg->imageData[(y * hsvImg->widthStep) + x * 3 + 1] >= saturation)
							{
								if (hsvImg->imageData[(y * hsvImg->widthStep) + x * 3 + 2] >= brightness)
									maskImg->imageData[(y * maskImg->widthStep) + x] = color_table_camera[hsvImg->imageData[(y * hsvImg->widthStep) + x * 3]];
								else 
									maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
							}
							else maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
						}
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)
						{
							if(dipSwitchNew[1] == 64 && origColor & maskImg->imageData[y * maskImg->widthStep + x])
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = 0;
							}
							else if(origColor & maskImg->imageData[y * maskImg->widthStep + x]) 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = (unsigned short)(((int)hsvImg->imageData[y * hsvImg->widthStep + x * 3] + colorChangeVal) % 180);
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2];
							}
							else 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = hsvImg->imageData[y * hsvImg->widthStep + x * 3];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2];
							}
						}
					cvCvtColor(hsvImg, cameraImg, CV_HSV2BGR);
					cvResize(cameraImg, resizeImg, CV_INTER_NN);
					cvIMG2RGB_fullscreen(resizeImg, cis_rgb_big, 640, 480);
					fb_display_camera(cis_rgb_big);
				}
				
				// only camera flag(0, 0)
				else
				{
					*addr_segment_select = 0x00;
					// camera read & img process
					write(dev, NULL, 1);
					read(dev, cis_rgb, 320*240*2);
					RGB2cvIMG(cameraImg, cis_rgb, 320, 240);
					cvResize(cameraImg, resizeImg, CV_INTER_NN);
					cvIMG2RGB_fullscreen(resizeImg, cis_rgb_big, 640, 480);
					fb_display_camera(cis_rgb_big);
				}
			}
			cvReleaseImage(&hsvImg);
			cvReleaseImage(&maskImg);
			cvReleaseImage(&cameraImg);
			cvReleaseImage(&resizeImg);
			*addr_segment_select = 0x00;
		}
	
		// image mode 진입
 		else if(mode == 1)
		{
			Fill_Background(0x0000);
			// Initializing 
			printf("\n");
			printf(" --------------------------------------\n");
			printf(" --------------------------------------\n"); 
			printf("          entering image mode  \n");
			printf(" --------------------------------------\n");
			printf(" --------------------------------------\n\n");
			
			backgroundImg = cvLoadImage("image_isolation.bmp", 1);
			if(backgroundImg == NULL)
			{
				printf("There is no such loading Image file. Please check again\n");
			}
			cvIMG2RGB_fullscreen(backgroundImg, cis_rgb_back, 800, 480);
			fb_display_fullscreen(cis_rgb_back);
			cvReleaseImage(&backgroundImg);

			// name input
			printf("Input the file name\n");
			count = 0;
			fileName[0] = '\0';
			while(count < 29 && ch != '\n')
			{
				if(kbhit())
				{ 
					ch = readch();
					fileName[count] = ch;
					if(fileName[count] == '\n') count--;
					else if(fileName[count] == '\b') count--;
					count++;
				}
			}
			printf("\n");
			fileName[count] = '\0';
			printf("%s\n", fileName);
			image = cvLoadImage(fileName, 1);
				
			// if fail
			if(image == NULL) 
			{ 
				printf("There is no such file. Please check again\n"); 
				continue;
			}
	
			// Image matrixes define
			resizeImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
			hsvImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
			maskImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 1);
			
			// Image resizing
			cvResize(image, resizeImg, CV_INTER_LINEAR);
			cvReleaseImage(&image);
	
			// Image display(original image)
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
			*addr_dip_select = 1;
			dipSwitchOrig[0] = *addr_dip_data;
			*addr_dip_select = 0;
			dipSwitchOrig[1] = *addr_dip_data;
			ch = 0;
			image = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
			saturation = S_LIMIT;
			brightness = V_LIMIT;
			keypad_flag = 0;
			while(ch != 'q')
			{
				if(kbhit()) ch = readch();
				if(keypadhit()) 
				{
					if(keypad_flag < 3) keypad_flag++;
					if(keypad_flag == 1) keypadValChange(&saturation, &brightness);
				}
				else keypad_flag = 0;

				display_segment(saturation, brightness);
				*addr_dip_select = 1;
				dipSwitchNew[0] = *addr_dip_data;
				*addr_dip_select = 0;
				dipSwitchNew[1] = *addr_dip_data;
				if(dipSwitchOrig[0] != dipSwitchNew[0] || keypad_flag == 1) 
				{
					dipSwitchOrig[0] = dipSwitchNew[0];
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)
						{
							if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 1] >= saturation)
							{
								if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 2] >= brightness)
									maskImg->imageData[(y * maskImg->widthStep) + x] = color_table[resizeImg->imageData[(y * resizeImg->widthStep) + x * 3]];
								else 
									maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
							}
							else maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
						}			
					for(y = 0; y < 240; y++)
						for(x = 0; x < 320; x++)								{
							if(dipSwitchOrig[0] & maskImg->imageData[y * maskImg->widthStep + x]) 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 1];
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 2];
							}
							else 
							{
								hsvImg->imageData[y * hsvImg->widthStep + x * 3] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 1] = 0;
								hsvImg->imageData[y * hsvImg->widthStep + x * 3 + 2] = 0;
							}
						}
					cvCvtColor(hsvImg, image, CV_HSV2BGR);
					cvIMG2RGB565(image, cis_rgb, 320, 240);
					fb_display(cis_rgb, 435, 120);
				}
			}
			cvReleaseImage(&hsvImg);
			cvReleaseImage(&image);
			cvReleaseImage(&resizeImg);
			cvReleaseImage(&maskImg);
			*addr_segment_select = 0x00;
		}
	}
}