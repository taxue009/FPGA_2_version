#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
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

#include <mach/gpio.h>

//理解file_operations、file、inode 等内核结构的作用 和 联系 ！！！

static dev_t addr_devt;
static struct class *cls;

//设备结构体
struct addr{
	struct cdev cdev;
	struct device *class_dev;
};
struct addr addr_dev;

/**
int irq_open(struct inode *inode,struct file *file)
{
	//打开设备后的初始操作
	struct addr *chip = container_of(inode->i_cdev,struct addr,cdev);
	file->private_data = chip;
	return 0;
}
*/
/**
ssize_t irq_read(struct file *file,char __user *buf,size_t size,loff_t *offset ){}
*/
ssize_t addr_write(struct file *file,const char __user *buf,size_t size, loff_t *offset){
	uint16_t *addr;
	int i;
	
	copy_from_user(addr,buf,sizeof(uint16_t));
	/**
	for(i=0;i<5;i++){
		if(*addr & (0x1 << i))
			at91_set_gpio_value(data->rt_id[i],1);
		else
			at91_set_gpio_value(data->rt_id[i],0);
	}
	*/
	void __iomem *pioa_codr,*pioa_sodr;
	pioa_codr = (void __iomem*)(0xFEF78000) +0x87400 + 0x34;
	pioa_sodr = (void __iomem*)(0xFEF78000) +0x87400 + 0x30;
	__raw_writel(0x1F << 23,pioa_codr);
	__raw_writel((*addr & 0x1F)<<23,pioa_sodr);
	
	void __iomem *piob_codr,*piob_sodr;
	piob_sodr = (void __iomem*)(0xFEF78000) +0x87600 + 0x30;
	piob_codr = (void __iomem*)(0xFEF78000) +0x87600 + 0x34;
	
	__raw_writel(0x01000000,piob_codr);
	__raw_writel(0x40000000,piob_sodr);
	
	__raw_writel(0x01000000,piob_sodr);
	__raw_writel(0x40000000,piob_codr);
	
	return size; 
}

//file_operations 是设备编号 与 驱动程序的 连接口
static struct file_operations addr_fops = {
	.owner = THIS_MODULE,
	//.open = irq_open,
	//.release = NULL,
	//.read = irq_read,
	.write = addr_write,
};



static int addr_drv_init(void){
	int ret;
	
	//待解决！！！
	cls = class_create(THIS_MODULE, "confaddr_class");
	if(IS_ERR(cls))
		{
		  return PTR_ERR(cls);
		}
	
	//①获取设备编号
	ret = alloc_chrdev_region(&addr_devt, 0, 2, "my_addr");
	if(ret < 0){
		class_destroy(cls);
		return ret;
	}
	
	//②注册设备驱动
	cdev_init(&addr_dev.cdev,&addr_fops);
	addr_dev.cdev.owner = THIS_MODULE;
	addr_dev.cdev.ops = &addr_fops;
	ret = cdev_add(&addr_dev.cdev,addr_devt,1);	
	if(ret<0){
	  printk(KERN_ALERT "error\n");	
	}
	
	//③创建设备节点
	addr_dev.class_dev = device_create(cls,NULL,addr_devt,&addr_dev,"myaddr");	
	if(IS_ERR(addr_dev.class_dev)){
	  printk(KERN_ALERT "error\n");	
	}
	
	return 0;
}

static int addr_drv_exit(void){
	//①注销设备节点
	device_unregister(addr_dev.class_dev);
	//②注销设备
	cdev_del(&addr_dev);
	//③释放设备编号
	unregister_chrdev_region(addr_devt,2);
	
	return 0;
}

module_init(addr_drv_init);
module_exit(addr_drv_exit);

MODULE_LICENSE("GPL"); 