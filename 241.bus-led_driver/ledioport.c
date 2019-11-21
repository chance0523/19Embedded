// 모듈의 헤더파일 선언
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/ioport.h>

#define DRIVER_AUTHOR "hanback"
#define DRIVER_DESC "led test program"
#define LEDIOPORT_MAJOR 241
#define LEDIOPORT_NAME "LED IO PORT"
#define LEDIOPORT_MODULE_VERSION
#define LEDIOPORT_ADDRESS 0x88000020
#define LEDIOPORT_ADDRESS_RANGE 0x1000

//Global variable
static int ledioport_usage = 0;			 //드라이버가 사용여부를 확인하는 값
static unsigned long *ledioport_ioremap; //IO주소공간저장

int ledioport_open(struct inode *minode, struct file *mfile)
{
	// 디바이스가 열려 있는지 확인.

	if (ledioport_usage != 0)
		return -EBUSY;

	// LED의 가상 주소 매핑
	ledioport_ioremap = ioremap(LEDIOPORT_ADDRESS, LEDIOPORT_ADDRESS_RANGE);
	// 등록할 수 있는 I/O 영역인지 확인값

	if (!check_mem_region((unsigned long)ledioport_ioremap, LEDIOPORT_ADDRESS_RANGE))
	{
		// I/O 메모리 영역을 등록
		request_mem_region((unsigned long)ledioport_ioremap, LEDIOPORT_ADDRESS_RANGE, LEDIOPORT_NAME);
	}

	else
		printk(KERN_WARNING "Can't get IO Region 0x%x\n", (unsigned int)ledioport_ioremap);

	ledioport_usage = 1;

	return 0;
}

// 응용 프로그램에서 디바이스를 더이상 사용하지 않아서 닫기를 구현하는 함수
int ledioport_release(struct inode *minode, struct file *mfile)
{ // 매핑된 가상주소를 해제

	iounmap(ledioport_ioremap);
	// 등록된 I/O 메모리 영역을 해제
	release_mem_region((unsigned long)ledioport_ioremap, LEDIOPORT_ADDRESS_RANGE);

	ledioport_usage = 0;
	return 0;
}

// 디바이스 드라이버의 쓰기를 구현하는 함수
ssize_t ledioport_write_byte(struct file *inode, const char *gdata, size_t length, loff_t *off_what)
{
	unsigned short *addr;
	unsigned char c;
	// gdata 의 사용자 공간의 메모리에서 c 에 읽어온다.
	get_user(c, gdata);
	addr = (unsigned short *)ledioport_ioremap;
	*addr = c | 0x100;
	return length;
}

// 파일 오퍼레이션 구조체
// 파일을 열때 open()을 사용한다. open()는 시스템 콜을 호출하여 커널 내부로 들어간다.
// 해당 시스템 콜과 관련된 파일 연산자 구조체 내부의 open 에 해당하는 필드가 드라이버
// 내에서 ledioport_open()으로 정의되어 있으므로 ledioport_open()가 호출된다.
// write 와 release 도 마찬가지로 동작한다. 만약 등록되지 않은 동작에 대해서는 커널에서
// 정의해 놓은 default 동작을 하도록 되어 있다.
static struct file_operations led_fops = {

	.owner = THIS_MODULE,
	.open = ledioport_open,
	.write = ledioport_write_byte,
	.release = ledioport_release,

};

// 모듈을 커널 내부로 삽입
// 모듈 프로그램의 핵심적인 목적은 커널 내부로 들어가서 서비스를 제공받는 것이므로
// 커널 내부로 들어가는 init()을 먼저 시작한다.
// 응용 프로그램은 소스 내부에서 정의되지 않은 많은 함수를 사용한다. 그것은 외부
// 라이브러리가 컴파일 과정에서 링크되어 사용되기 때문이다. 모듈 프로그램은 커널
// 내부하고만 링크되기 때문에 커널에서 정의하고 허용하는 함수만을 사용할 수 있다.
int led_init(void)
{
	int result;
	// 문자 디바이스 드라이버를 등록한다.
	result = register_chrdev(LEDIOPORT_MAJOR, LEDIOPORT_NAME, &led_fops);
	if (result < 0)
	{ // 등록실패
		printk(KERN_WARNING "Can't get any major\n");
		return result;
	} // major 번호를 출력한다.
	printk(KERN_INFO "Init Module, ledioport Major Number : %d\n", LEDIOPORT_MAJOR);
	return 0;
}

//모듈을 커널에서 제거
void led_exit(void)
{
	// 문자 디바이스 드라이버를 제거한다.
	unregister_chrdev(LEDIOPORT_MAJOR, LEDIOPORT_NAME);
	printk(KERN_INFO "driver: %s DRIVER EXIT\n", LEDIOPORT_NAME);
}

module_init(led_init); // 모듈 적재 시 호출되는 함수
module_exit(led_exit); // 모듈 제거 시 호출되는 함수

MODULE_AUTHOR(DRIVER_AUTHOR);	// 모듈의 저작자
MODULE_DESCRIPTION(DRIVER_DESC); // 모듈에 대한 설명
MODULE_LICENSE("Dual BSD/GPL");  // 모듈의 라이선스 등록
