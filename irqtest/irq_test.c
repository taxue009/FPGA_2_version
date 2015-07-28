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

static dev_t irq_devt;
static struct class *cls;

//设备结构体
struct irq{
	struct cdev cdev;
	struct device *class_dev;
	int irq;
	struct work_struct irq_wq;//内核工作队列结构体
	int data;
	spinlock_t irq_lock;
};
struct irq irq_dev;


int irq_open(struct inode *inode,struct file *file)
{
	//打开设备后的初始操作
	struct irq *chip = container_of(inode->i_cdev,struct irq,cdev);
	file->private_data = chip;
	return 0;
}

ssize_t irq_read(struct file *file,char __user *buf,size_t size,loff_t *offset ){
	struct irq *idev = file->private_data;	
	copy_to_user(buf,&idev->data,sizeof(int));
	return size;
}
//ssize_t irq_write(struct file *file,const char __user *buf,size_t size, loff_t *offset){}

static irqreturn_t irq_interrupt(int irq,void *dev_id)
{
	struct irq *idev = dev_id;
	spin_lock_irq(&idev->irq_lock);
	idev->data++;
	spin_unlock_irq(&idev->irq_lock);
	//schedule_work(&idev->irq_wq);
	return IRQ_HANDLED;
}

static int irq_do_work(struct work_struct *work)
{
	struct irq *idev = container_of(work,struct irq,irq_wq);
	return 1;
	//中断底部 将读取的数据缓存
}

//file_operations 是设备编号 与 驱动程序的 连接口
static struct file_operations irq_fops = {
	.owner = THIS_MODULE,
	//.open = irq_open,
	//.release = NULL,
	.read = irq_read,
	//.write = irq_write,
};



static int irq_drv_init(void){
	int ret;
	//struct irq *irq_dev;
	//irq_dev = kmalloc(sizeof(struct irq),GFP_KERNEL);
	
	//待解决！！！
	cls = class_create(THIS_MODULE, "myirq_class");
	if(IS_ERR(cls))
		{
		  return PTR_ERR(cls);
		}
	
	//①获取设备编号
	ret = alloc_chrdev_region(&irq_devt, 0, 2, "my_irq");
	if(ret < 0){
		class_destroy(cls);
		return ret;
	}
	
	//②注册设备驱动
	//&irq_dev.cdev = cdev_alloc();
	cdev_init(&irq_dev.cdev,&irq_fops);
	irq_dev.cdev.owner = THIS_MODULE;
	irq_dev.cdev.ops = &irq_fops;
	ret = cdev_add(&irq_dev.cdev,irq_devt,1);	
	if(ret<0){
	  printk(KERN_ALERT "error\n");	
	}
	
	//③创建设备节点
	irq_dev.class_dev = device_create(cls,NULL,irq_devt,&irq_dev,"myIRQ");	
	if(IS_ERR(irq_dev.class_dev)){
	  printk(KERN_ALERT "error\n");	
	}
	
	//申请中断
	void __iomem* aic_ffer;
	aic_ffer=(void __iomem*)(0xFEF78000) +0x87000 + 0x140;
	__raw_writel(0x00000008,aic_ffer);
	
	spin_lock_init(&irq_dev.irq_lock);
	irq_dev.data = 0;
	INIT_WORK(&irq_dev.irq_wq,irq_do_work);
	irq_dev.irq = AT91_PIN_PB9;
	at91_set_gpio_input(irq_dev.irq,1);
	at91_set_deglitch(irq_dev.irq,1);
	ret = request_irq(irq_dev.irq,irq_interrupt,IRQ_TYPE_EDGE_RISING ,"my_irq",&irq_dev);
	
	return 0;
}

static int irq_drv_exit(void){
	//中断释放
	free_irq(irq_dev.irq,&irq_dev);
	//①注销设备节点
	device_unregister(irq_dev.class_dev);
	//②注销设备
	cdev_del(&irq_dev);
	//③释放设备编号
	unregister_chrdev_region(irq_devt,2);
	
	return 0;
}

module_init(irq_drv_init);
module_exit(irq_drv_exit);

MODULE_LICENSE("GPL"); 