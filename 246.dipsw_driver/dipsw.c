// 모듈의 헤더파일 선언 
#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/fs.h> 
#include <linux/errno.h> 
#include <linux/types.h> 
#include <asm/fcntl.h> 
#include <linux/ioport.h>

#include <asm/ioctl.h> 
#include <asm/uaccess.h> 
#include <asm/io.h>

#define DRIVER_AUTHOR 				"hanback" 		//모듈의저작자 
#define DRIVER_DESC 				"dipsw program" // 모듈에 대한 설명
#define DIP_MAJOR_NUMBER		 246 							// 디바이스 주번호 
#define DIP_NAME 						"DIPSW"					 // 디바이스 이름 
#define DIP_MODULE_VERSION	"DIPSW V0.1"		 // 디바이스 버전
#define ADDR_DIP_PHY				 0x88000062 		// dipsw의 물리주소

#define DIP_ADDRESS_RANGE 0x1000  // I/O 영역의 크기
#define TIMER_INTERVAL 20

//Global variable

static unsigned int dipsw_usage = 0; 
static unsigned long dipsw_ioremap; 
static unsigned short *dipaddr; 
static unsigned short *dipaddr1;
static unsigned short value[4];

// define functions...
// 응용 프로그램에서 디바이스를 처음 사용하는 경우를 처리하는 함수
int dipsw_open (struct inode *inode, struct file *filp)
 {
// 디바이스가 열려 있는지 확인. 
		if(dipsw_usage != 0) return -EBUSY;
// dipsw의 가상 주소 매핑
		dipsw_ioremap = (unsigned long)ioremap(ADDR_DIP_PHY, DIP_ADDRESS_RANGE);
// 등록할 수 있는 I/O 영역인지 확인 
		if(!check_mem_region(dipsw_ioremap,DIP_ADDRESS_RANGE)) 
			{
				// I/O 메모리 영역을 등록
				request_mem_region(dipsw_ioremap, DIP_ADDRESS_RANGE, DIP_NAME);
		  }
		  
		else printk("driver:unabletoregisterthis!\n");
			
// dipsw 의 첫번째줄과 두번째줄에 대한 주소 설정
		dipaddr =(unsigned short*)dipsw_ioremap; 
		dipaddr1 =(unsigned short*)(dipsw_ioremap+0x2);
		dipsw_usage = 1;
		
		return 0;
 }
 
  
// 응용 프로그램에서 디바이스를 더이상 사용하지 않아서 닫기를 구현하는 함수 
int dipsw_release (struct inode *inode, struct file *filp) 
	{
 		// 매핑된 가상주소를 해제
		iounmap((unsigned long*)dipsw_ioremap);
		// 등록된 I/O 메모리 영역을 해제 
		
		release_mem_region(dipsw_ioremap, DIP_ADDRESS_RANGE);
		dipsw_usage = 0;
		
		return 0; 
	}
	
// 디바이스 드라이버의 읽기를 구현하는 함수
ssize_t dipsw_read(struct file *inode, char *gdata, size_t length, loff_t *off_what)
 {
		int ret;
		// dipsw의 첫번째
		value[0] = ((*dipaddr & 0x00f0)>>4) | ((*dipaddr & 0x000f)<<4);
		// dipsw의 두번째
		value[1]=((*dipaddr1 & 0xf000)>>8)>>4 | ((*dipaddr1 & 0x0f00)>>8)<<4;
		
		// value 가 가리키는 커널 메모리 데이터를 gdata 가 가리키는 사용자 
		// 메모리 데이터에 n 바이트 만큼 써넣는다.
		ret=copy_to_user(gdata,value,4); 
		if(ret<0) return -1;
			
		return length; 
		}
		
// 파일 오퍼레이션 구조체
// 파일을 열때 open()을 사용한다. open()는 시스템 콜을 호출하여 커널 내부로 들어간다. 
// 해당 시스템 콜과 관련된 파일 연산자 구조체 내부의 open 에 해당하는 필드가 드라이버 내에서 
// dipsw_open()으로 정의되어 있으므로 dipsw_open()가 호출된다.
// read, release도 마찬가지로 동작한다.
struct file_operations dipsw_fops = {
																				.owner = THIS_MODULE, 
																				.open = dipsw_open, 
																				.read = dipsw_read, 
																				.release = dipsw_release,
																		};
																		
// 모듈을 커널 내부로 삽입
// 모듈 프로그램의 핵심적인 목적은 커널 내부로 들어가서 서비스를 제공받는 것이므로 
// 커널 내부로 들어가는 init()을 먼저 시작한다.
// 응용 프로그램은 소스 내부에서 정의되지 않은 많은 함수를 사용한다. 그것은 외부 
// 라이브러리가 컴파일 과정에서 링크되어 사용되기 때문이다. 모듈 프로그램은 커널 
// 내부하고만 링크되기 때문에 커널에서 정의하고 허용하는 함수만을 사용할 수 있다.
int dipsw_init(void) 
	{
			int result;
			// 문자 디바이스 드라이버를 등록한다.
			result = register_chrdev(DIP_MAJOR_NUMBER, DIP_NAME, &dipsw_fops); if (result < 0) 
				{ // 등록실패
					printk(KERN_WARNING"Can't get any major\n");
					return result;
				}
			// major 번호를 출력한다.
			printk("Init Module, Dipsw Major Number : %d\n", DIP_MAJOR_NUMBER);
			 
			return 0;
	}		
	
// 모듈을 커널에서 제거 
void dipsw_exit(void) 
	{
			// 문자 디바이스 드라이버를 제거한다. 
			unregister_chrdev(DIP_MAJOR_NUMBER,DIP_NAME);
			printk("driver: %s DRIVER EXIT\n", DIP_NAME);
  }				
  
module_init(dipsw_init); // 모듈 적재 시 호출되는 함수
module_exit(dipsw_exit); // 모듈 제거 시 호출되는 함수
MODULE_AUTHOR(DRIVER_AUTHOR); // 모듈의 저작자
MODULE_DESCRIPTION(DRIVER_DESC); // 모듈에 대한 설명 
MODULE_LICENSE("Dual BSD/GPL"); // 모듈의 라이선스 등록  													
