// 모듈의 헤더파일 선언
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <asm/fcntl.h>
#include <linux/ioport.h>
#include <linux/delay.h>

#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define DRIVER_AUTHOR "hanback"
#define DRIVER_DESC "7-Segment program"
#define SEGMENT_MAJOR 242
#define SEGMENT_NAME "SEGMENT"
#define SEGMENT_MODULE_VERSION "SEGMENT PORT V0.1" // 디바이스 버전
#define SEGMENT_ADDRESS_GRID 0x88000030			   // 7-seg 의 Digit 를 선택하기 위한 레지스터
#define SEGMENT_ADDRESS_DATA 0x88000032			   // 7-seg 를 디스플레이 하기 위한 레지스터
#define SEGMENT_ADDRESS_1 0x88000034			   // 7-seg 를 디스플레이 하기 위한 레지스터
#define SEGMENT_ADDRESS_RANGE 0x1000			   // I/O 영역의 크기
#define MODE_0_TIMER_FORM 0x0					   // 출력 포맷 제어 변수
#define MODE_1_CLOCK_FORM 0x1

//Global variable
static unsigned int segment_usage = 0;
static unsigned long *segment_data;
static unsigned long *segment_grid;
static int mode_select = 0x0;

// define functions...
// 응용 프로그램에서 디바이스를 처음 사용하는 경우를 처리하는 함수
int segment_open(struct inode *inode, struct file *filp)
{
	// 디바이스가 열려 있는지 확인.
	if (segment_usage != 0)
		return -EBUSY;
	// GRID와DATA의 가상 주소 매핑
	segment_grid = ioremap(SEGMENT_ADDRESS_GRID, SEGMENT_ADDRESS_RANGE);
	segment_data = ioremap(SEGMENT_ADDRESS_DATA, SEGMENT_ADDRESS_RANGE);
	// 등록할 수 있는 I/O 영역인지 확인
	if (!check_mem_region((unsigned long)segment_data, SEGMENT_ADDRESS_RANGE) && !check_mem_region((unsigned long)segment_grid, SEGMENT_ADDRESS_RANGE))
	{
		// I/O 메모리 영역을 등록
		request_region((unsigned long)segment_grid, SEGMENT_ADDRESS_RANGE, SEGMENT_NAME);
		request_region((unsigned long)segment_data, SEGMENT_ADDRESS_RANGE, SEGMENT_NAME);
	}
	else
		printk("driver : unable to register this!\n");

	segment_usage = 1;
	return 0;
}

// 응용 프로그램에서 디바이스를 더이상 사용하지 않아서 닫기를 구현하는 함수
int segment_release(struct inode *inode, struct file *filp)
{
	// 매핑된 가상주소를 해제
	iounmap(segment_grid);
	iounmap(segment_data);
	// 등록된 I/O 메모리 영역을 해제
	release_region((unsigned long)segment_data, SEGMENT_ADDRESS_RANGE);
	release_region((unsigned long)segment_grid, SEGMENT_ADDRESS_RANGE);
	segment_usage = 0;
	return 0;
}

// x에 대한 LED의 코드로 변환하여 반환
unsigned short Getsegmentcode(short x)
{
	unsigned short code;
	switch (x)
	{
	case 0x0:
		code = 0xfc;
		break;
	case 0x1:
		code = 0x60;
		break;
	case 0x2:
		code = 0xda;
		break;
	case 0x3:
		code = 0xf2;
		break;
	case 0x4:
		code = 0x66;
		break;
	case 0x5:
		code = 0xb6;
		break;
	case 0x6:
		code = 0xbe;
		break;
	case 0x7:
		code = 0xe4;
		break;
	case 0x8:
		code = 0xfe;
		break;
	case 0x9:
		code = 0xf6;
		break;
	case 0xa:
		code = 0xfa;
		break;
	case 0xb:
		code = 0x3e;
		break;
	case 0xc:
		code = 0x1a;
		break;
	case 0xd:
		code = 0x7a;
		break;
	case 0xe:
		code = 0x9e;
		break;
	case 0xf:
		code = 0x8e;
		break;
	default:
		code = 0;
		break;
	}
	return code;
}

// 디바이스 드라이버의 쓰기를 구현하는 함수
ssize_t segment_write(struct file *inode, const char *gdata, size_t length, loff_t *off_what)
{
	unsigned char data[6];
	unsigned char digit[6] = {0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
	unsigned int i, num, ret;
	unsigned int count = 0, temp1, temp2;

	// 사용자 메모리 gdata를 커널 메모리 num으로 n만큼 복사
	ret = copy_from_user(&num, gdata, 4);
	count = num;
	if (num != 0)
	{ // num 이 0 이 아닐때
		data[5] = Getsegmentcode(count / 100000);
		temp1 = count % 100000;
		data[4] = Getsegmentcode(temp1 / 10000);
		temp2 = temp1 % 10000;
		data[3] = Getsegmentcode(temp2 / 1000);
		temp1 = temp2 % 1000;
		data[2] = Getsegmentcode(temp1 / 100);
		temp2 = temp1 % 100;
		data[1] = Getsegmentcode(temp2 / 10);
		data[0] = Getsegmentcode(temp2 % 10);
		switch (mode_select)
		{

		case MODE_0_TIMER_FORM:
			break;
		case MODE_1_CLOCK_FORM:
			// dot print
			data[4] += 1;
			data[2] += 1;
			break;
		}
		// print
		for (i = 0; i < 6; i++)
		{
			*segment_grid = digit[i];
			*segment_data = data[i];
			mdelay(1);
		}
	}
	*segment_grid = ~digit[0];
	*segment_data = 0;
	return length;
}

static int segment_ioctl(struct inode *inode, struct file *flip, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
	case MODE_0_TIMER_FORM:
		mode_select = 0x00;
		break;
	case MODE_1_CLOCK_FORM:
		mode_select = 0x01;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

// 파일 오퍼레이션 구조체
// 파일을 열때 open()을 사용한다. open()는 시스템 콜을 호출하여 커널 내부로 들어간다.
// 해당 시스템 콜과 관련된 파일 연산자 구조체 내부의 open 에 해당하는 필드가 드라이버 내에서
// segment_open()으로 정의되어 있으므로 segment_open()가 호출된다.
// write 와 release 도 마찬가지로 동작한다. 만약 등록되지 않은 동작에 대해서는 커널에서 정의해
// 놓은 default 동작을 하도록 되어 있다.
struct file_operations segment_fops = {
	.owner = THIS_MODULE,
	.open = segment_open,
	.write = segment_write,
	.release = segment_release,
	.ioctl = segment_ioctl,
};

// 모듈을 커널 내부로 삽입
// 모듈 프로그램의 핵심적인 목적은 커널 내부로 들어가서 서비스를 제공받는 것이므로
// 커널 내부로 들어가는 init()을 먼저 시작한다.
// 응용 프로그램은 소스 내부에서 정의되지 않은 많은 함수를 사용한다. 그것은 외부
// 라이브러리가 컴파일 과정에서 링크되어 사용되기 때문이다. 모듈 프로그램은 커널
// 내부하고만 링크되기 때문에 커널에서 정의하고 허용하는 함수만을 사용할 수 있다.
int segment_init(void)
{
	int result;
	// 문자 디바이스 드라이버를 등록한다.
	result = register_chrdev(SEGMENT_MAJOR, SEGMENT_NAME, &segment_fops);
	if (result < 0)
	{
		printk(KERN_WARNING "Can't get any major\n");
		return result;
	}
	// major 번호를 출력한다.
	printk(KERN_INFO "Init Module, 7-Segment Major Number : %d\n", SEGMENT_MAJOR);
	return 0;
}

// 모듈을 커널에서 제거
void segment_exit(void)
{
	// 문자 디바이스 드라이버를 제거한다.
	unregister_chrdev(SEGMENT_MAJOR, SEGMENT_NAME);
	printk("driver: %s DRIVER EXIT\n", SEGMENT_NAME);
}

module_init(segment_init); // 모듈 적재 시 호출되는 함수
module_exit(segment_exit); // 모듈 제거 시 호출되는 함수

MODULE_AUTHOR(DRIVER_AUTHOR);	//모듈의 저작
MODULE_DESCRIPTION(DRIVER_DESC); // 모듈에 대한 설명
MODULE_LICENSE("Dual BSD/GPL");  // 모듈의 라이선스 등록
