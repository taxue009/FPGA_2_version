#include <linux/string.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
/*硬件相关的头文件*/
#include <mach/at91sam9_smc.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/at91sam9260_matrix.h>
#include <mach/at91sam9260.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

//#include "sam9_smc.h"

/*这个是硬件（CPU）密切相关的中断号*/
#include<mach/irqs.h>

static dev_t fpga_devt;
static struct class *cls;

struct atmel_fpga_data{
	int fpga_irq;
	int fpga_int_a;
	int fpga_idle;
	int fpga_read_en;
	int fpga_read_over;
	
	//**增加14.12.23
	int rt_id[5];
	int rst_n;
	int rt_id_en;
	//*/
	
	int fpga_data[16];
};

/*PIO配置*/
void __init at91_add_device_fpga(struct atmel_fpga_data *data){
	//unsigned long csa;

	if (!data)
		return -1;
	//at91_sys_write();//使能PIOC的时钟
	
	if(data->fpga_int_a)
		at91_set_gpio_output(data->fpga_int_a,0);
	if(data->fpga_idle)
		at91_set_gpio_input(data->fpga_idle,0);
	if(data->fpga_read_en)
		at91_set_gpio_output(data->fpga_read_en,0);
	if(data->fpga_read_over)
		at91_set_gpio_input(data->fpga_read_over,0);
	if(data->fpga_irq){
		at91_set_gpio_input(data->fpga_irq,1);
		at91_set_deglitch(data->fpga_irq,1);
	}
	
	volatile unsigned long *pio_pdsr;
	pio_pdsr = ioremap(0xFFFFF43C,4);
	printk(KERN_ALERT "PDSR:0x%08x\n",*pio_pdsr);
	
	at91_set_gpio_output(data->fpga_data[0],0);
	at91_set_gpio_output(data->fpga_data[1],0);
	at91_set_gpio_output(data->fpga_data[2],0);
	at91_set_gpio_output(data->fpga_data[3],0);
	at91_set_gpio_output(data->fpga_data[4],0);
	at91_set_gpio_output(data->fpga_data[5],0);
	at91_set_gpio_output(data->fpga_data[6],0);
	at91_set_gpio_output(data->fpga_data[7],0);
	at91_set_gpio_output(data->fpga_data[8],0);
	at91_set_gpio_output(data->fpga_data[9],0);
	at91_set_gpio_output(data->fpga_data[10],0);
	at91_set_gpio_output(data->fpga_data[11],0);
	at91_set_gpio_output(data->fpga_data[12],0);
	at91_set_gpio_output(data->fpga_data[13],0);
	at91_set_gpio_output(data->fpga_data[14],0);
	at91_set_gpio_output(data->fpga_data[15],0);
	printk(KERN_ALERT "PDSR:0x%08x\n",*pio_pdsr);
	//待补充时钟设置
	
	//**增加14.12.23
	at91_set_gpio_output(data->rt_id[4],0);
	at91_set_gpio_output(data->rt_id[3],0);
	at91_set_gpio_output(data->rt_id[2],0);
	at91_set_gpio_output(data->rt_id[1],0);
	at91_set_gpio_output(data->rt_id[0],0);
	if(data->rst_n)
		at91_set_gpio_output(data->rst_n,1);
	if(data->rt_id_en)
		at91_set_gpio_output(data->rt_id_en,0);
}

static struct atmel_fpga_data __initdata fpga_data = {
	.fpga_irq = AT91_PIN_PA20,             //待补充
	.fpga_int_a = AT91_PIN_PB27,
	.fpga_idle = AT91_PIN_PB25,       // PIN50  修改
	.fpga_read_en = AT91_PIN_PB26,      //待确认
	.fpga_read_over = AT91_PIN_PB22,
	
	.fpga_data[0] = AT91_PIN_PA4,
	.fpga_data[1] = AT91_PIN_PA5,
	.fpga_data[2] = AT91_PIN_PA6,
	.fpga_data[3] = AT91_PIN_PA7,
	.fpga_data[4] = AT91_PIN_PA8,
	.fpga_data[5] = AT91_PIN_PA9,
	.fpga_data[6] = AT91_PIN_PA10,
	.fpga_data[7] = AT91_PIN_PA11,
	.fpga_data[8] = AT91_PIN_PA12,
	.fpga_data[9] = AT91_PIN_PA13,
	.fpga_data[10] = AT91_PIN_PA14,
	.fpga_data[11] = AT91_PIN_PA15,
	.fpga_data[12] = AT91_PIN_PA16,
	.fpga_data[13] = AT91_PIN_PA17,
	.fpga_data[14] = AT91_PIN_PA18,
	.fpga_data[15] = AT91_PIN_PA19,
	
	//**增加14.12.23
	.rt_id[4] = AT91_PIN_PA27,
	.rt_id[3] = AT91_PIN_PA26,
	.rt_id[2] = AT91_PIN_PA25,
	.rt_id[1] = AT91_PIN_PA24,
	.rt_id[0] = AT91_PIN_PA23,
	.rst_n = AT91_PIN_PB24,
	.rt_id_en = AT91_PIN_PB30,
};

/*硬件资源量，这是根据开发板确定的*/
static struct resource atmel_fpga_resource[]=
{
	/*IRQ*/
    [0]=
    {
        .flags = IORESOURCE_IRQ,
        .start = AT91_PIN_PA20,
        .end = AT91_PIN_PA20,
        .name = "fpga_int",
    },
    /*MEM */
	/**
    [1]=
    {
        .flags = IORESOURCE_MEM,//待补充
        .start = 0xFFFE4000,
        .end = 0xFFFFBFFF,
        .name = "fpga_mem",
    },
	
	[1]=
    {
        .flags = IORESOURCE_MEM,//待补充
        .start = 0xFFFFF434,
        .end = 0xFFFFF437,
        .name = "pio_codr",
    },
	[2]=
	{
		.flags = IORESOURCE_MEM,
		.start = 0xFFFFF430,
		.end = 0xFFFFF433,
		.name = "pio_sodr",
	},
	[3]=
	{
		.flags = IORESOURCE_MEM,
		.start = 0xFFFFF43C,
		.end   = 0xFFFFF43F,
		.name = "pio_pdsr",
	},
	*/
};

static struct platform_device fpga_device=
{
    /*设备名*/
    .name = "myfpga",
    .id = -1,
	.dev		= {
				.platform_data	= &fpga_data,
	},
    /*资源数*/
    .num_resources = ARRAY_SIZE(atmel_fpga_resource),
    /*资源指针*/
    .resource = atmel_fpga_resource,
};

//** 增加 14.12.23
void __init config_rtaddr(struct atmel_fpga_data *data){
	printk(KERN_ALERT "yes2!!\n");
	static char buf[8];
	
	struct file *fp;
    mm_segment_t fs;
    loff_t pos;
    printk(KERN_ALERT "configuration\n");
    fp = filp_open("/home/fpga_addr.txt", O_RDWR , 0);
    if (IS_ERR(fp)) {
        printk(KERN_ALERT "create file error\n");
    }
	
    fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
	
    //vfs_write(fp, buf, sizeof(buf), &pos);
    //pos = 0;
	
    vfs_read(fp, buf, sizeof(buf), &pos);
	int x=0;
	while(x < 8){
		printk(KERN_ALERT "RECV: x=%d 0x%02x\n",x,buf[x]);
		x++;
	}

    filp_close(fp, NULL);
    set_fs(fs);
	
	uint16_t *addr;
	int i;
	addr = (uint16_t *)&buf[4];
	for(i=0;i<5;i++){
		if(*addr & (0x1 << i))
			at91_set_gpio_value(data->rt_id[i],1);
		else
			at91_set_gpio_value(data->rt_id[i],0);
	}
	/**
	at91_set_gpio_value(data->rst_n,0);
	at91_set_gpio_value(data->rt_id_en,1);
	ndelay(63);
	at91_set_gpio_value(data->rst_n,1);
	at91_set_gpio_value(data->rt_id_en,0);
	*/
	/**
	volatile unsigned long *piob_sodr,*piob_codr;
	piob_sodr = ioremap(0xFFFFF630,4);
	piob_codr = ioremap(0xFFFFF634,4);
	*piob_codr |= 0x01000000;

	*piob_sodr = (*piob_sodr) | (0x40000000);
	*piob_sodr = (*piob_sodr) | (0x01000000);
	*piob_codr |= 0x40000000;
	*/
	void __iomem *piob_codr,*piob_sodr;
	piob_sodr = (void __iomem*)(0xFEF78000) +0x87600 + 0x30;
	piob_codr = (void __iomem*)(0xFEF78000) +0x87600 + 0x34;
	
	__raw_writel(0x01000000,piob_codr);
	__raw_writel(0x40000000,piob_sodr);
	
	__raw_writel(0x01000000,piob_sodr);
	__raw_writel(0x40000000,piob_codr);
}
//*/

static int __init atmel_fpga_init(void)
{
    int ret ;
	
	cls = class_create(THIS_MODULE, "fpga_class");
	if (IS_ERR(cls))
		{return PTR_ERR(cls);
		 printk(KERN_ALERT "NO1!!\n");}
	ret = alloc_chrdev_region(&fpga_devt,0,1,"my_fpga");
	device_create(cls, NULL, MKDEV(MAJOR(fpga_devt),0),NULL, "my_fpga");
	
	at91_add_device_fpga(&fpga_data);
	printk(KERN_ALERT "yes1!!\n");
	
	//**  增加 14.12.23
	config_rtaddr(&fpga_data);
	printk(KERN_ALERT "yes3!!\n");
	//*/
	
    /*设备注册*/
    platform_device_register(&fpga_device);
	return 0;
}

static void __exit atmel_fpga_exit(void)
{
    /*设备的注销*/
	device_destroy(cls,MKDEV(MAJOR(fpga_devt),0));
    class_destroy(cls);
    platform_device_unregister(&fpga_device);
}

/*加载与卸载*/
module_init(atmel_fpga_init);
module_exit(atmel_fpga_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rick Bronson");
MODULE_DESCRIPTION("FPGA/SmartMedia driver for AT91 / AVR32");
MODULE_ALIAS("platform:atmel_FPGA");
