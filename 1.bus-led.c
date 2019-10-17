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

static struct termios initial_settings, new_setings;
static int peek_character = -1;

void init_keyboard()
{
    tcgetattr(0, &initial_settings);
    new settings = initial_settings;
    new_setings.c_lflag &= ~ICANON;
    new_setings.c_lflag &= ~ECHO;
    new_setings.c_lflag &= ~ISIG;
    new_setings.c_cc[VMIN] = 1;
    new_setings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_setings);
}

void close_keyboar()
{
    tcsetattr(0, TCSANOW, &initial_settings);
}

int kbhit()
{
    char ch;
    int nread;
    if (peek_character != -1)
        return -1;
    new_setings.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &new_setings);
    nread = read(0, &ch, 1);
    new_settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new_setings);
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