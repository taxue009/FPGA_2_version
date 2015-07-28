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
#include <stdint.h>
#define data_max 32
int main(int argc, char *argv[]){
	int fpga_device=-1,i=0;
	char go,c;
	uint16_t fpga_data = 1,fpga_read;
	fpga_device=open(argv[1],O_RDWR);
	if (fpga_device < 0){
		printf("open:fpga_device failed\n");
		exit(-1);
    }
	
	pid_t pid;
	pid = fork();	
	if (pid < 0) { 
		fprintf(stderr, "Error in fork!\n"); 
    } else if (pid == 0){
		while(1){
			read(fpga_device,&fpga_read,sizeof(uint16_t));
				printf("data:0x%04x\n",fpga_read);
		}
	}else{
		while(1){
			write(fpga_device,&fpga_data,sizeof(uint16_t));
			fpga_data++;
			scanf("%c",&go);
			while( (c = getchar())!='\n' && c!=EOF);
		}
	}
	
	return 1;
}
