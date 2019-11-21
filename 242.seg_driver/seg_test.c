#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <fcntl.h> 
#include <time.h> 
#include <termios.h>
//Global variable
static struct termios initial_settings, new_settings; // 키보드 입력 상태 제어를 위한 변수
static int peek_character = -1;
// 키보드 입력 상태 제어를 위한 함수들 
void init_keyboard() 
{
	 tcgetattr(0,&initial_settings);
	 new_settings = initial_settings; 
	 new_settings.c_lflag &= ~ICANON; 
	 new_settings.c_lflag &= ~ECHO; 
	 new_settings.c_lflag &= ~ISIG; 
	 new_settings.c_cc[VMIN] = 1; 
	 new_settings.c_cc[VTIME] = 0; 
	 tcsetattr(0,TCSANOW,&new_settings);
}

void close_keyboard() 
{
	 tcsetattr(0,TCSANOW,&initial_settings);
}

int kbhit() 
{ 
	char ch;
	int nread;
	if(peek_character != -1) return 1;
		
	new_settings.c_cc[VMIN] = 0; 
	tcsetattr(0,TCSANOW,&new_settings); 
	nread = read(0,&ch,1); 
	new_settings.c_cc[VMIN] = 1; 
	tcsetattr(0,TCSANOW,&new_settings);
	
		if(nread == 1) 
			{ 
				peek_character = ch; return 1;
			}
			
return 0; 

}

int readch() 
{
	 char ch;
	 
	 if(peek_character != -1) 
	 	{
			ch = peek_character; 
			peek_character = -1; 
			return ch;
		} 
		
		read(0,&ch,1); 
		return ch;
}

int main() 
{
	int fd, value=1, i, count, clock,ch; 
	unsigned short input, dir =0;
// /dev/segment 파일을 읽기/쓰기 액세스 모드로 열어
// fd 변수에 파일 기술자를 저장한다. 

	if((fd=open("/dev/segment",O_RDWR|O_SYNC)) <= 0) 
		 {
				printf("FND open fail\n");
				exit(1);
		 }
		 
// keyboard input mode change
init_keyboard();
printf(" --------------------------------------\n"); 
printf(" 7Segment IO Interface Procedure\n"); 
printf(" --------------------------------------\n"); 
printf(" [c] counter\n");
printf(" [t] system time\n");
printf(" [q] exit\n");
printf(" --------------------------------------\n\n");

while (input != 'q') 
	{
	 	 input = 'r'; 
	 			if(kbhit()) 
	 				{
							ch = readch(); 
							switch(ch) 
								{
									case 'c': input = 'c'; break; 
									case 't': input = 't'; break; 
									case 'q': input = 'q'; break;
								}
					 }	
							switch (input) 
								{ 	
									
									case 'c':	
										
										printf("Input counter value (0 : exit program, 10 = 1sec ) : "); 
										close_keyboard();											
										scanf("%d", &value);
										init_keyboard();
										count = value;
											
printf(" --------------------------------------\n");
printf(" Counter\n");
printf(" --------------------------------------\n");
printf(" [p] pause\n");
printf(" [c] continue\n");
printf(" [r] reset\n");
printf(" [q] exit\n");
printf(" --------------------------------------\n\n");

// format setting 
			ioctl(fd, 0, NULL, NULL); 
			dir = 'c';
			while(dir != 'q') 
			{
				 if(dir == 'c')
					 { 
				 			if (count <= 0)
							break;
							// continue
 							for(i=0;i<14;i++)
							write(fd,&count,4);
					 	count--;
						}
				else if(dir=='r')
					{ 
						// reset
						count = value;
						dir = 'c';
					}
				else if(dir=='p')
					{
						for(i=0;i<14;i++)
						write(fd,&count,4);
				  }
				  
				if(kbhit()) 
					{ 
						ch = readch(); 
						
						switch(ch)
						 {
								case 'c': dir = 'c'; break;
								case 'p': dir = 'p'; break; 
								case 'r': dir = 'r'; break;
								case 'q': dir = 'q'; break;
					   }
				  }
				}
printf(" --------------------------------------\n"); 
printf(" 7Segment IO Interface Procedure\n"); 
printf(" --------------------------------------\n"); 
printf(" [c] counter\n");
printf(" [t] system time\n");
printf(" [q] exit\n");
printf(" --------------------------------------\n\n");
 break;
 
case 't':
	
printf(" --------------------------------------\n");
printf(" System Time\n");
printf(" --------------------------------------\n"); 
printf(" [c] continue\n");
printf(" [p] pause\n");
printf(" [q] exit\n");
printf(" --------------------------------------\n\n");

// 시간을 받아올 구조체 
	struct timeval val; 
	struct tm *ptm;
	
// printing format setting 
	ioctl(fd, 1, NULL, NULL); 
	dir = 'c';

	while(dir != 'q')
	 { 
	 		if(dir == 'c')
	 			 {
// 시간 받아오기 
						gettimeofday(&val, NULL);
						ptm = localtime(&val.tv_sec);
						clock = ptm->tm_hour * 10000 + ptm->tm_min * 100 + ptm->tm_sec;
						write(fd, &clock, 4);
				 }
				 
			else if(dir=='p')
				{
						write(fd, &clock, 4);
			  }
			  
			if(kbhit()) 
				{ 
					ch = readch(); 
						switch(ch) 
							{
								case 'p': dir = 'p'; break;
								case 'c': dir = 'c'; break;
								case 'q': dir = 'q'; break;
						  } 
			  }
   }
   
printf(" --------------------------------------\n"); 
printf(" 7Segment IO Interface Procedure\n"); 
printf(" --------------------------------------\n"); 
printf(" [c] counter\n");
printf(" [t] system time\n");
printf(" [q] exit\n");
printf(" --------------------------------------\n\n");

 	break;

case 'q': 
	
	break;
	
								} //switch (input) 
								
  } //while (input != 'q') 

	close_keyboard();
	close(fd); // 파일을 닫는다 
	return 0;
	
}//main
 
 
 
 											
