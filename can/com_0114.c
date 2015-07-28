#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>
#include <unistd.h>     /*Unix标准函数定义*/
#include <sys/types.h>  /**/
#include <sys/stat.h>   /**/
#include <sys/wait.h>	/*waitpid*/
#include <fcntl.h>      /*文件控制定义*/
#include <termios.h>    /*PPSIX终端控制定义*/
#include <errno.h>      /*错误号定义*/
#include <getopt.h>
#include <string.h>

#define FALSE 1
#define TRUE 0

char *recchr="We received:\"";

int speed_arr[] = { 
	B921600, B460800, B230400, B115200, B57600, B38400, B19200, 
	B9600, B4800, B2400, B1200, B300, B38400, B19200, B9600, 
	B4800, B2400, B1200, B300, 
};
int name_arr[] = {
	921600, 460800, 230400, 115200, 57600, 38400,  19200,  
	9600,  4800,  2400,  1200,  300, 38400,  19200,  9600, 
	4800, 2400, 1200,  300, 
};

void set_speed(int fd, int speed)
{
	int   i;
	int   status;
	struct termios   Opt;
	tcgetattr(fd, &Opt);            //取得终端介质（fd）初始值，并把其值 赋给temios_p;

	for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) {
		if  (speed == name_arr[i])	{
			tcflush(fd, TCIOFLUSH);/*丢弃要写入引用的对象，但是尚未传输的数据，或者收到但是尚未读取的数据，取决于 queue_selector 的值： 
                                                              TCIFLUSH ：刷新收到的数据但是不读  
                                                              TCOFLUSH ：刷新写入的数据但是不传送  
                                                              TCIOFLUSH ：同时刷新收到的数据但是不读，并且刷新写入的数据但是不传送  
                                   */
			cfsetispeed(&Opt, speed_arr[i]);//设置 termios 结构中存储的输入波特率为 speed。如果输入波特率被设为0，实际输入波特率将等于输出波特率。
			cfsetospeed(&Opt, speed_arr[i]);//设置 termios_p 指向的 termios 结构中存储的输出波特率为 speed
			status = tcsetattr(fd, TCSANOW, &Opt);/*设置与终端相关的参数 (除非需要底层支持却无法满足)，使用 termios_p 引用的 termios 结构。
			                                      optional_actions （tcsetattr函数的第二个参数）指定了什么时候改变会起作用： 
                                                  TCSANOW：改变立即发生  
                                                  TCSADRAIN：改变在所有写入 fd 的输出都被传输后生效。这个函数应当用于修改影响输出的参数时使用。(当前输出完成时将值改变)  
                                                  TCSAFLUSH ：改变在所有写入 fd 引用的对象的输出都被传输后生效，所有已接受但未读入的输入都在改变发生前丢弃(同TCSADRAIN，但会舍弃当前所有值)。
                                                  */
			if  (status != 0)
				perror("tcsetattr fd1");//输出错误原因
				return;
		}
		tcflush(fd,TCIOFLUSH);
   }
}
/*
	*@brief   设置串口数据位，停止位和效验位
	*@param  fd     类型  int  打开的串口文件句柄*
	*@param  databits 类型  int 数据位   取值 为 7 或者8*
	*@param  stopbits 类型  int 停止位   取值为 1 或者2*
	*@param  parity  类型  int  效验类型 取值为N,E,O,,S
        *@param  flowctrl   类型 int 流控开关 取值为0或1
*/
int set_Parity(int fd,int databits,int stopbits,int parity,int flowctrl)
{
	struct termios options;
	if  ( tcgetattr( fd,&options)  !=  0) {
		perror("SetupSerial 1");
		return(FALSE);
	}
	options.c_cflag &= ~CSIZE ;//屏蔽其他标志位
	switch (databits) /*设置数据位数*/ {
	case 7:
		options.c_cflag |= CS7;
	break;
	case 8:
		options.c_cflag |= CS8;
	break;
	default:
		fprintf(stderr,"Unsupported data size\n");
		return (FALSE);
	}
	
	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;   /* Clear parity enable */
		options.c_iflag &= ~INPCK;     /* Enable parity checking */
	break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);  /* 设置为奇效验*/
		options.c_iflag |= INPCK;             /* Disnable parity checking */
	break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;     /* Enable parity */
		options.c_cflag &= ~PARODD;   /* 转换为偶效验*/ 
		options.c_iflag |= INPCK;       /* Disnable parity checking */
	break;
	case 'S':	
	case 's':  /*as no parity*/
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
	break;
	default:
		fprintf(stderr,"Unsupported parity\n");
		return (FALSE);
	}
 	/* 设置停止位*/  
  	switch (stopbits) {
   	case 1:
    	options.c_cflag &= ~CSTOPB;
  	break;
 	case 2:
  		options.c_cflag |= CSTOPB;
  	break;
 	default:
  		fprintf(stderr,"Unsupported stop bits\n");
  		return (FALSE);
 	}
	/* Set flow control */
	if (flowctrl)
		options.c_cflag |= CRTSCTS;
	else
		options.c_cflag &= ~CRTSCTS;

  	/* Set input parity option */
  	if (parity != 'n')
    	options.c_iflag |= INPCK;
  	options.c_cc[VTIME] = 150; // 15 seconds
    options.c_cc[VMIN] = 0;

	options.c_lflag &= ~(ECHO | ICANON);
	
	//补充  不能接收0x03的问题
	options.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	options.c_oflag &= ~OPOST;
	options.c_cflag |= CLOCAL | CREAD;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  	tcflush(fd,TCIFLUSH); /* Update the options and do it NOW */
  	if (tcsetattr(fd,TCSANOW,&options) != 0) {
    	perror("SetupSerial 3");
  		return (FALSE);
 	}
	return (TRUE);
}

/**
	*@breif 打开串口
*/
int OpenDev(char *Dev)
{
	int fd = open( Dev, O_RDWR );         //| O_NOCTTY | O_NDELAY   O_RDWR表示读写打开
 	if (-1 == fd) { /*设置数据位数*/
   		perror("Can't Open Serial Port");
   		return -1;
	} else
		return fd;
}


/* The name of this program */
const char * program_name;

/* Prints usage information for this program to STREAM (typically
 * stdout or stderr), and exit the program with EXIT_CODE. Does not
 * return.
 */

void print_usage (FILE *stream, int exit_code)
{
    fprintf(stream, "Usage: %s option [ dev... ] \n", program_name);
    fprintf(stream,
            "\t-h  --help     Display this usage information.\n"
            "\t-d  --device   The device ttyS[0-3] or ttyEXT[0-3]\n"
            "\t-s  --string   Write the device data\n"
	    "\t-f  --flow     Flow control switch\n"
	    "\t-b  --speed    Set speed bit/s\n");
    exit(exit_code);
}



/*
	*@breif  main()
 */
int main(int argc, char *argv[])
{
	FILE *fp;

    int  fd, next_option, havearg = 0;
    char *device = "/dev/ttySAC2"; /* Default device */
	int speed = 115200;
	int flowctrl = 0;

 	int nread;			/* Read the counts of data */
 	char buff[10];		/* Recvice data buffer */
	pid_t pid;
	char *xmit = "1234567890"; /* Default send data */ 

    /*** ext Uart Test program ***/
    //char recv_buff[512];
    unsigned long total = 0;
    unsigned long samecnt = 0;

    const char *const short_options = "h:d:b:s:f";

    const struct option long_options[] = {
        { "help",   0, NULL, 'h'},
        { "device", 1, NULL, 'd'},
        { "string", 1, NULL, 's'},
	{ "speed",  1, NULL, 'b'},
	{ "flow",   0, NULL, 'f'},
        { NULL,     0, NULL, 0  }
    };

	/**
    program_name = argv[0];

    do {
        next_option = getopt_long (argc, argv, short_options, long_options, NULL);//
        switch (next_option) {
            case 'h':
                print_usage (stdout, 0);
            case 'd':
                device = optarg;
				havearg = 1;
				break;
            case 's':
                xmit = optarg;
				havearg = 1;
				break;
			case 'b':
				speed = atoi (optarg);
				havearg = 1;
				break;
			case 'f':
				flowctrl = 1;
				break;
            case -1:
				if (havearg)  break;
            case '?':
                print_usage (stderr, 1);
            default:
                abort ();
        }
    }while(next_option != -1);
	*/
	
 	sleep(1);
 	fd = OpenDev(device);

 	if (fd > 0) {
      	set_speed(fd,speed);
 	} else {
	  	fprintf(stderr, "Error opening %s: %s\n", device, strerror(errno));
  		exit(1);
 	}
	if (set_Parity(fd,8,1,'N', flowctrl)== FALSE) {
		fprintf(stderr, "Set Parity Error\n");
		close(fd);
      	exit(1);
   	}
	
	pid = fork();	
	if (pid < 0) { 
		fprintf(stderr, "Error in fork!\n"); 
    } else if (pid == 0){
		while(1) {
			printf("child ID\n");
                        sleep (1);
			nread = read(fd, buff, sizeof(buff));
			if (nread > 0) {
				buff[nread] = '\0';
				int x=0;
				while(x < nread){
					printf("RECV: x=%d 0x%02x\n",x,buff[x]);
					x++;
				}
				write(fd,buff,9);
			}
			
			if((fp = fopen("/home/fpga_addr.txt","w")) == NULL)
				printf("open error \n");
			fwrite(&buff[1],sizeof(char),8,fp);
			fclose(fp);
		}
		exit(0);
    } else { 
		while(1) {
			printf("father ID\n");
			pid_t pr = waitpid(pid, NULL, WNOHANG);
			if(pr == pid) return 0;		// Success
			scanf("%s",xmit);
			printf("SEND: %s\n",xmit);
			write(fd, xmit, strlen(xmit));

                        sleep (1);
		}
    }

    close(fd);
    exit(0);
}

