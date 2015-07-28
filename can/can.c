/*
 * can_test.c
 * 
 * 2007/04/28 Mengrz
 * CAN BUS test for EBD9260 board.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <pthread.h>
#include <sys/signal.h>

#include "can.h"
#include "Queue.h"

#define data_max 32

LinkQueue *Qcan;
LinkQueue *Qfpg;
pthread_mutex_t fpg_queue,can_queue;

struct message{
	uint16_t command;
	uint16_t data[data_max];
};

static void can_show_header(struct can_frame_header *header)
{
    printf("\nHeader: id=%d srr=%d ide=%d eid=%d rtr=%d rb1=%d rb0=%d dlc=%d\n",
	   header->id,
	   header->srr,
	   header->ide,
	   header->eid,
	   header->rtr,
	   header->rb1,
	   header->rb0,
	   header->dlc);
}

static int device = -1;
static int device1 = -1;
static int fpga_device = -1;

static void die(int x)
{
    close(device);
    close(device1);
	close(fpga_device);
    exit(0);
}

static void *can_read(void * th) 
{
    struct can_frame frame_recv;
    static sid = 0;
    int i;
	
    while(1) {
		memset(&frame_recv, 0, sizeof(struct can_frame));
		if (device1 > 0)
		    read(device1, &frame_recv, sizeof(struct can_frame));
		else
		    read(device, &frame_recv, sizeof(struct can_frame));
	
		if (frame_recv.header.id != sid) {
		    can_show_header(&frame_recv.header);
		    printf("RECV: \"");
		}
		for (i=0; i<frame_recv.header.dlc; i++){
		    printf("%c", frame_recv.data[i]);
			pthread_mutex_lock(&can_queue);
			if(EnQueue(Qcan,frame_recv.data[i]))
			printf("can读取成功\n");
			pthread_mutex_unlock(&can_queue);
		}
		if (frame_recv.header.id != sid)
		    printf("\"\n");
			
		sid = frame_recv.header.id;
    }
    return NULL;
}


static void *fpg_write()
{
	unsigned char fpg_data[2];
	char x;
	int i;
	//struct message fpga_write;
	//uint16_t command = 0;
	while(1){
		pthread_mutex_lock(&can_queue);
		if(DeQueue(Qcan,&fpg_data[0])){
			if(DeQueue(Qcan,&fpg_data[1])){
				x = fpg_data[0];
				fpg_data[0] = fpg_data[1];
				fpg_data[1] = x;
				write(fpga_device, fpg_data, sizeof(uint16_t));
			}else{
				EnQueue(Qcan,fpg_data[0]);
			}
		}
		pthread_mutex_unlock(&can_queue);
		
		//fpga_write.command = command;
		//memcpy(fpga_write.data,fpg_data,8);
	}
}


static void *fpg_read()
{
	//struct message fpga_read;
	//uint16_t command = 0;
	int x,i;
	unsigned char fpg_data[2];
	while(1){
		//memset(&fpga_read,0,sizeof(struct message));
		read(fpga_device, fpg_data, sizeof(uint16_t));
		//if(fpga_read.command == command)
			//break;
		//x = fpga_read.command & 0x1F;
		//for(i=0;i<x && i+4 <= x;i+=4){
			//memcpy(fpg_data,fpga_read.data+i,8);
			//有问题  fpg_data[9] = '\0';
			pthread_mutex_lock(&fpg_queue);
			if(EnQueue(Qfpg,fpg_data[1]))
				printf("fpga读取成功\n");
			if(EnQueue(Qfpg,fpg_data[0]))
				printf("fpga读取成功\n");
			pthread_mutex_unlock(&fpg_queue);
		//}
		//if(x%4){
			//memcpy(fpg_data,fpga_read.data+i,2*(x%4));
			//fpg_data[2*(x%4)] = '\0';
			//pthread_mutex_lock(&fpg_queue);
			//if(EnQueue(Qfpg,fpg_data,command))
				//printf("fpga读取成功\n");
			//pthread_mutex_unlock(&fpg_queue);
		//}
	}
}

static void *can_write()
{
	struct can_frame fram_send;
	char can_data[8];
	int i;
	int Qfpg_can_id = 1;
	//uint16_t command;
	while(1){
		i=0;
		pthread_mutex_lock(&fpg_queue);
	    while( i<8 ){
			if(IsEmpty(Qfpg))
				break;
			else
				DeQueue(Qfpg,&can_data[i]);
			i++;
		}
	    pthread_mutex_unlock(&fpg_queue);
		
		if(i != 0){
		
			memset(&fram_send, 0, sizeof(struct can_frame));
			//fram_send.header.id=解析fpga命令 找出对应can_ID;
			fram_send.header.id = Qfpg_can_id;
		
			fram_send.header.ide=0;
			fram_send.header.eid=0;
		
			fram_send.header.dlc = i;
			memcpy(fram_send.data,can_data,fram_send.header.dlc);
		
			write(device,&fram_send,sizeof(struct can_frame));
			Qfpg_can_id++;
			if(Qfpg_can_id > 2031)
				Qfpg_can_id = 1;
		}
		//delay();
	}
	
}

void delay(void)
{
	int i =10000;
	while(i--);	
}

int main(int argc, char *argv[])
{	
	Qcan =( LinkQueue *) malloc(sizeof(LinkQueue));
	Qfpg = (LinkQueue *)malloc(sizeof(LinkQueue));
	printf("yes\n");
    InitQueue(Qcan);
    InitQueue(Qfpg);
	printf("yes1\n");	
	pthread_mutex_init(&fpg_queue,NULL);
	pthread_mutex_init(&can_queue,NULL);
    printf("yes2\n");
    if (argc < 2) {
		printf("Useage: %s dev [[dev1 [-f]] [-f]]\n", argv[0]);
		printf("   dev dev1 --- can device\n");
		printf("   -f       --- filter mode\n");
		exit(0);
    }
    
    static char buf[256];
    static char *pbuf=buf;
    struct can_frame frame;
    
    struct can_filter filter = {
		.fid = {
		    {0,0,0,1},
		    {0,1,0,1},
		    {0,0,0,1},
		    {0,1,0,1},
		    {0,0,0,1},
		    {0,1,0,1},
		},
		.sidmask = 0x7E0,
		.eidmask = 0x3FFFF,
		.mode = 0,
    };
    
    struct can_filter not_filter = {
		.sidmask = 0,
		.eidmask = 0,
		.mode = 0,
    };
    printf("yes3\n");
    device = open(argv[1], O_RDWR); //  /dev/can0
	fpga_device = open("/dev/my_fpga2",O_RDWR);
    if (device < 0 || fpga_device < 0){
		perror("open:fpga_device");
		exit(-1);
    }
    if (argc > 2) {
		if (strcmp(argv[2], "-f") != 0) {
		    device1 = open(argv[2], O_RDWR);
		    if (device1 < 0) {
				perror("open:device1");
				exit(-1);
		    }
		}
    }	
    
    int i,can_id = 1;
    int rate;
    int len;
	printf("yes4\n");
    ioctl(device, CAN_IOCSFILTER, &not_filter);//不过滤
   	ioctl(device, CAN_IOCSRATE, 100000);//100k
    ioctl(device, CAN_IOCGRATE, &rate);
    printf("%s Work Rate: %dbps\n", argv[1], rate);
    
    if (device1 > 0) {
		ioctl(device1, CAN_IOCSFILTER, &not_filter);
		ioctl(device1, CAN_IOCSRATE, 100000);
		if (argc > 3) {
		    if (strcmp(argv[3], "-f") == 0) {
				ioctl(device1, CAN_IOCSFILTER, &filter);
		    }
		}
		printf("%s Work Rate: %dbps\n", argv[2], rate);
   	} else {	
		if (argc > 2) {
		    if (strcmp(argv[2], "-f") == 0) {
				ioctl(device, CAN_IOCSFILTER, &filter);
		    }
		}
#if 0
		ioctl(device, CAN_IOCTLOOPBACKMODE);
		printf("%s work on loopback mode.\n", argv[1]);
#else
		ioctl(device, CAN_IOCTNORMALMODE);
		printf("%s work on normal mode.\n", argv[1]);
#endif
    }
    
    pthread_attr_t attr;
    pthread_t thread_recv,can_send,fpg_recv,fpg_send;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	printf("yes5\n");
    if (pthread_create(&thread_recv, NULL, can_read, 0)) {
		perror("thread_create");
		exit(-1);
    }
	
    if (pthread_create(&can_send, NULL, can_write, 0)) {
		perror("thread_create");
		exit(-1);
    }
	
	 if (pthread_create(&fpg_send, NULL, fpg_write, 0)) {
		perror("thread_create");
		exit(-1);
    }
	
	 if (pthread_create(&fpg_recv, NULL, fpg_read, 0)) {
		perror("thread_create");
		exit(-1);
    }
    printf("yes6\n");
    if (((argc == 3) && (strcmp(argv[2], "-f") == 0)) || ((argc > 3) && (strcmp(argv[3], "-f") == 0))) {
		printf("\nTest in Filter mode. \n");
		printf("Only receive the frame that it's id & 0xf == [3...8]\n"); 
		printf("The frame data=id(id=1...2031), and eid=id*2+1\n");
    } else {
		printf("\nPress \"\\q\" to quit! \n");
		printf("Input the message and press Enter to send!\n");
		printf("The frame id=1...2031, and eid=id*2+1\n");
    }

    signal(SIGKILL, die);
    signal(SIGINT,  die);
    
    while(1)
    {
//		if (getchar() == 'q') break;
			
		memset(&frame, 0, sizeof(struct can_frame));
	
		frame.header.id = can_id;
	
		if ((can_id % 2) == 0) {
		    frame.header.ide = 1;
		    frame.header.eid = can_id * 2 + 1;
		}else{
		    frame.header.ide = 0;
		    frame.header.eid = 0;
		}
		
		if (((argc > 2) && (strcmp(argv[2], "-f") == 0)) || ((argc > 3) && (strcmp(argv[3], "-f") == 0))) {
		    sprintf(frame.data, "%d", can_id);
		    frame.header.dlc = strlen(frame.data);
		    write(device, &frame, sizeof(struct can_frame));
		    sleep(1);
		}
		else {
		 	    
	 	    printf("\nCanTest> ");
		    scanf("%s", buf);
		    if (strcmp(buf, "\\q") == 0)
				break;
		    len = strlen(buf);
		    pbuf = buf;
		  
		    for (len; len>CAN_FRAME_MAX_DATA_LEN;len-=CAN_FRAME_MAX_DATA_LEN) {
				memcpy(frame.data, pbuf, CAN_FRAME_MAX_DATA_LEN);
				frame.header.dlc = CAN_FRAME_MAX_DATA_LEN;
				write(device, &frame, sizeof(struct can_frame));
				pbuf += CAN_FRAME_MAX_DATA_LEN;
				delay();
	    	}
	   		if (len > 0) {
	   			
				frame.header.dlc = len;
				memcpy(frame.data, pbuf, len);
				write(device, &frame, sizeof(struct can_frame));
	    	}
		}
		can_id++;
		if (can_id > 2031) {
	    	can_id = 1;
		}
    }
    
    close(device);
    if (device1 > 0)
		close(device1);
    return 0;
}
