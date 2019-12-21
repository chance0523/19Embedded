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

#define FPGA_BASEADDRESS 0x88000000
#define DIP_DATA_READ_12 0x60
#define DIP_DATA_OFFSET 0x62
#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define FBDEV_FILE "/dev/fb0"
#define CAMERA_DEVICE "/dev/camera"
#define FILE_NAME "save.jpg"

#define H_RED_ORANGE 13
#define H_ORANGE_YELLOW 27
#define H_YELLOW_GREEN 40
#define H_GREEN_CYAN 80
#define H_CYAN_BLUE 100
#define H_BLUE_VIOLET 130
#define H_VIOLET_RED 165
#define S_LIMIT 150
#define V_LIMIT 150

unsigned char color_table[181] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,           //  0 -  9
    1, 1, 1, 1, 2, 2, 2, 2, 2, 2,           // 10 - 19
    2, 2, 2, 2, 2, 2, 2, 2, 4, 4,           // 20 - 29
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,           // 30 - 39
    4, 8, 8, 8, 8, 8, 8, 8, 8, 8,           // 40 - 49
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,           // 50 - 59
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,           // 60 - 69
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,           // 70 - 79
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // 80 - 89
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // 90 - 99
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, // 100 - 109
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, // 110 - 119
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, // 120 - 129
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 130 - 139
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 140 - 149
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, // 150 - 159
    64, 64, 64, 64, 64, 1, 1, 1, 1, 1,      // 160 - 169
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,           // 170 - 179
    1};

static CvMemStorage *storage = 0;
struct fb_var_screeninfo fbvar;
unsigned char *pfbmap;
unsigned short cis_rgb[320 * 240 * 2];
unsigned short cis_rgb_big[640 * 480 * 2];

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

int fb_display_fullscreen(unsigned short *rgb)
{
    int coor_x, coor_y;
    int screen_width;
    unsigned short *ptr;

    screen_width = fbvar.xres;

    for (coor_y = 0; coor_y < 480; coor_y++)
    {
        ptr = (unsigned short *)pfbmap + (screen_width * coor_y);
        for (coor_x = 0; coor_x < 640; coor_x++)
        {
            *ptr++ = rgb[coor_x + coor_y * 640];
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
            cv_rgb[y * 640 + x] = (unsigned short)RGB565(r, g, b);
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
            *(unsigned short *)(pfbmap + (x)*2 + (y)*800 * 2) = color;
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

int main(int argc, char **argv)
{
    int fbfd, fd;
    int dev, ret = 0;
    int x, y;
    int count, mode;
    int optlen = strlen("--cascade=");
    unsigned short ch = 0;
    unsigned short dipSwitchNew[2], dipSwitchOrig[2];
    unsigned short *addr_fpga, *addr_dip_data, *addr_dip_select;
    char fileName[30];
    CvCapture *capture = 0;
    IplImage *image = NULL;
    IplImage *cameraImg;
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

    // camera open / fail check
    dev = open(CAMERA_DEVICE, O_RDWR);
    if (dev < 0)
    {
        printf("Error: cannot open %s.\n", CAMERA_DEVICE);
        exit(1);
    }

    // dip switch memory mapping
    addr_fpga = (unsigned short *)mmap(NULL, 4096, PROT_WRITE, MAP_SHARED, fd, FPGA_BASEADDRESS);
    addr_dip_data = addr_fpga + DIP_DATA_OFFSET / sizeof(unsigned short);
    addr_dip_select = addr_fpga + DIP_DATA_READ_12 / sizeof(unsigned short);
    if (*addr_dip_select == (unsigned short)-1)
    {
        close(fd);
        printf("mmap error 1/n");
        exit(1);
    }

    if (*addr_dip_data == (unsigned short)-1)
    {
        close(fd);
        printf("mmap error 2\n");
        exit(1);
    }

    storage = cvCreateMemStorage(0);

    // choose mode
    init_keyboard();
    while (1)
    {
        Fill_Background(0x0011);
        printf(" --------------------------------------\n");
        printf("               Mode Select \n");
        printf(" --------------------------------------\n");
        printf(" [c] camera processing\n");
        printf(" [i] image processing\n");
        printf(" [q] exit\n");
        printf(" --------------------------------------\n\n");

        while (1)
        {
            if (kbhit())
            {
                ch = readch();
                if (ch == 'c')
                {
                    mode = 0;
                    break;
                }
                else if (ch == 'i')
                {
                    mode = 1;
                    break;
                }
                else if (ch == 'q')
                {
                    close_keyboard();
                    munmap(addr_fpga, 4096);
                    close(fd);
                    printf("\nGood bye\n");
                    return 0;
                }
                else
                    printf("Wrong Character. Type again!\n");
            }
        }

        if (mode == 0)
        {
            // Initializing
            printf("\n");
            printf(" --------------------------------------\n");
            printf(" --------------------------------------\n");
            printf("          entering camera mode \n");
            printf(" --------------------------------------\n");
            printf(" --------------------------------------\n\n");

            // image setting
            cameraImg = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
            resizeImg = cvCreateImage(cvSize(640, 480), IPL_DEPTH_8U, 3);

            // main operation
            *addr_dip_select = 1;
            dipSwitchOrig[0] = *addr_dip_data;
            *addr_dip_select = 0;
            dipSwitchOrig[1] = *addr_dip_data;
            ch = 0;
            while (ch != 'q')
            {
                if (kbhit())
                    ch = readch();

                // camera read & img process
                write(dev, NULL, 1);
                read(dev, cis_rgb, 320 * 240 * 2);
                RGB2cvIMG(cameraImg, cis_rgb, 320, 240);

                *addr_dip_select = 1;
                dipSwitchNew[0] = *addr_dip_data;
                *addr_dip_select = 0;
                dipSwitchNew[1] = *addr_dip_data;

                cvResize(cameraImg, resizeImg, CV_INTER_LINEAR);
                cvIMG2RGB_fullscreen(resizeImg, cis_rgb_big, 640, 480);
                fb_display_fullscreen(cis_rgb_big);
            }
            cvReleaseImage(&cameraImg);
            cvReleaseImage(&resizeImg);
        }

        else if (mode == 1)
        {
            // Initializing
            printf("\n");
            printf(" --------------------------------------\n");
            printf(" --------------------------------------\n");
            printf("          entering image mode  \n");
            printf(" --------------------------------------\n");
            printf(" --------------------------------------\n\n");

            // name input
            printf("Input the file name\n");
            count = 0;
            while (count < 29 && ch != '\n')
            {
                if (kbhit())
                {
                    ch = readch();
                    fileName[count] = ch;
                    if (fileName[count] == '\n')
                        count--;
                    else if (fileName[count] == '\b')
                        count--;
                    count++;
                }
            }
            printf("\n");
            fileName[count] = '\0';
            printf("%s\n", fileName);
            image = cvLoadImage(fileName, 1);

            // if fail
            if (image == NULL)
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
            for (y = 0; y < 240; y++)
                for (x = 0; x < 320; x++)
                {
                    if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 1] > S_LIMIT) //a.Vec<3b>(y,x)
                    {
                        if (resizeImg->imageData[(y * resizeImg->widthStep) + x * 3 + 2] > V_LIMIT)
                            maskImg->imageData[(y * maskImg->widthStep) + x] =
                                color_table[resizeImg->imageData[(y * resizeImg->widthStep) + x * 3]];
                        else
                            maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
                    }
                    else
                        maskImg->imageData[(y * maskImg->widthStep) + x] = 0;
                }

            // Main operation
            *addr_dip_select = 1;
            dipSwitchOrig[0] = *addr_dip_data;
            *addr_dip_select = 0;
            dipSwitchOrig[1] = *addr_dip_data;
            ch = 0;
            image = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
            while (ch != 'q')
            {
                if (kbhit())
                    ch = readch();
                *addr_dip_select = 1;
                dipSwitchNew[0] = *addr_dip_data;
                *addr_dip_select = 0;
                dipSwitchNew[1] = *addr_dip_data;
                if (dipSwitchOrig[0] != dipSwitchNew[0])
                {
                    printf("%02X %02X\n", dipSwitchNew[0], dipSwitchNew[1]);
                    dipSwitchOrig[0] = dipSwitchNew[0];
                    for (y = 0; y < 240; y++)
                        for (x = 0; x < 320; x++)
                        {
                            if (dipSwitchOrig[0] & maskImg->imageData[y * maskImg->widthStep + x])
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
        }
    }
}
