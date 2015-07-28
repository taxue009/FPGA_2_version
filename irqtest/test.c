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
	uint16_t fpga_data=0;
		while(1){
			printf("%d",fpga_data);
			fpga_data++;
			scanf("%c",&go);
			//fflush();
		}
	return 1;
}
