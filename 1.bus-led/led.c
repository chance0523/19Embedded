#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#define FPGA_BASEADDRESS 0x88000000
#define LED_OFFSET 0x20

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
        peek_character = 1;
        return ch;
    }
    read(0, &ch, 1);
    return ch;
}

int main()
{
    int fd;
    unsigned short *addr_fpga, *addr_led;
    unsigned short dir = '0';
    unsigned short val, val2;
    char ch = 'l';
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
    {
        perror("mem open fail n");
        exit(1);
    }
    addr_fpga = (unsigned short *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FPGA_BASEADDRESS);
    addr_led = addr_fpga + LED_OFFSET / sizeof(unsigned short);
    if (*addr_led == (unsigned short)-1)
    {
        close(fd);
        printf("mmap error n");
        exit(1);
    }
    init_keyboard();
    printf(" -------------------------------------- \n");
    printf(" 8bit LED IO Interface Procedure \n");
    printf(" -------------------------------------- \n");
    printf(" [l] left shift \n");
    printf(" [r] right shift \n");
    printf(" [q] exit \n");
    printf(" --------------------------------------\n\n");
    *addr_led = 0x100;
    val = 0;
    val2 = 0;
    while (dir != 'q')
    {
        if (dir == 'l')
        {
            val2 = (~(val >> 5)) & 0x1;
            val = (val << 1) | val2;
        }
        else if (dir == 'r')
        {
            val2 = (~(val << 5)) & 0x80;
            val = (val >> 1) | val2;
        }
        *addr_led = val | 0x100;
        usleep(80000);
        if (kbhit())
        {
            ch = readch();
            switch (ch)
            {
            case 'r':
                dir = 'r';
                break;
            case 'l':
                dir = 'l';
                break;
            case 'q':
                dir = 'q';
                break;
            }
        }
    }
    *addr_led = 0x00;
    close_keyboard();
    munmap(addr_fpga, 4096);
    close(fd);
    return 0;
}