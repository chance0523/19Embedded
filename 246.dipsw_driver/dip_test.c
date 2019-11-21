#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

// argc : 외부입력 단어 개수, argv: 외부입력 데이터

int main(int argc, char **argv)
{
	int dev;
	int f_loop = 1;
	unsigned short vkey_org[2], vkey_new[2], vkey_exit[2];

	//외부입력 단어수가 3 이상이 아니라면 메세지 출력후 종료

	if (argc <= 2)
	{
		printf("please input the parameter! ex)./test 12 48\n");
		return -1;
	}
	// /dev/dipsw 파일을 읽기 모드로 열어
	// dev 변수에 파일 기술자를 저장한다.
	if ((dev = open("/dev/dipsw", O_RDONLY)) < 0)
	{
		perror("DIPSW open fail\n");
		return -1;
	}

	if (argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X'))
		vkey_exit[0] = (unsigned short)strtol(&argv[1][2], NULL, 16);

	else
		vkey_exit[0] = (unsigned short)strtol(&argv[1][0], NULL, 16);

	if (argv[2][0] == '0' && (argv[2][1] == 'x' || argv[2][1] == 'X'))
		vkey_exit[1] = (unsigned short)strtol(&argv[2][2], NULL, 16);

	else
		vkey_exit[1] = (unsigned short)strtol(&argv[2][0], NULL, 16);

	printf("exit to : %02x %02x\n", vkey_exit[0], vkey_exit[1]);

	while (f_loop)
	{
		read(dev, &vkey_new, 4);
		if ((vkey_org[0] != vkey_new[0]) || (vkey_org[1] != vkey_new[1]))
		{
			printf("\t--- DIPSW ---\n");
			// vkey의 값을 읽는다.
			printf("\t> (SW1) : %02X\n", vkey_new[0]);
			printf("\t> (SW2) : %02X\n\n", vkey_new[1]);
			vkey_org[0] = vkey_new[0];
			vkey_org[1] = vkey_new[1];
		}
		if ((vkey_org[0] == vkey_exit[0]) && (vkey_org[1] == vkey_exit[1]))
			f_loop = 0;
	}

	close(dev); //파일을닫는다.

	return 0;
}
