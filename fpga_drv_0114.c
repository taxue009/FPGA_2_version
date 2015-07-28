#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>

#include <linux/jiffies.h>
#include <linux/delay.h>

#include <linux/kthread.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <mach/gpio.h>
#define data_max 64
//中断版 完全
static dev_t fpga_devt;
static struct class *cls;
struct task_struct *fpga_thread;
static int pin;

/**
//报文结构
struct message{
	short command;
	uint16_t data[4];
};
*/

//驱动结构体
struct fpga{
	struct cdev cdev;//字符驱动结构体 注册用
	struct device *class_dev;//驱动基本结构体 创建驱动
	struct platform_device *pfpga;//资源平台结构体
	
	//方案2
	uint16_t *io_addr_r;
	uint16_t *io_addr_w;
	
	void __iomem *io_read;
	void __iomem *io_write;
	void __iomem *io_clear;
	
	void __iomem *piob_sodr;
	void __iomem *piob_codr;
	void __iomem *piob_pdsr;

	uint16_t io_data;
	uint16_t r_data[data_max];
	uint16_t t_data[data_max];
	
	int r_in,r_out,t_in,t_out,r_off;
	int r_locked,t_locked;

	wait_queue_head_t iwq,owq,iqw,oqw; //等待队列头结点
	struct workqueue_struct *irq_queue;
	int irq_count;
	struct work_struct irq_wq;//内核工作队列结构体
	struct semaphore rblock,tblock;
	spinlock_t fpga_lock;
};

//中断底部 用工作队列实现调用
static int fpga_do_work(struct work_struct *work)
{
	printk(KERN_ALERT "interrupt3\n");
	struct fpga *chip = container_of(work,struct fpga,irq_wq);
	//struct atmel_fpga_data *fpga_data = chip->pfpga->dev.platform_data;
	uint16_t read_data;
	
	int count=chip->irq_count,num=0;
	chip->irq_count = 0;
	printk(KERN_ALERT "interrupt4\n");
		if(down_interruptible(&chip->rblock))
			return -ERESTARTSYS;
		while( (chip->r_in == chip->r_out) && (chip->r_locked == 1) ){
			up(&chip->rblock);
			if(wait_event_interruptible(chip->iwq, (chip->r_in != chip->r_out)))
				return -ERESTARTSYS;
			if (down_interruptible(&chip->rblock))
				return -ERESTARTSYS;
		}
		printk(KERN_ALERT "write_addr:%08x,%d\n",chip->io_addr_w,chip->r_off);
		if(chip->r_in < chip->r_out)
			num = chip->r_out - chip->r_in;
		else num = data_max + (chip->r_out - chip->r_in);
		//方案2
		if(count <= num){
			while(count){
				read_data = *(chip->io_addr_w+chip->r_off);
				printk(KERN_ALERT "write_data:%04x\n",read_data);
				chip->r_off += 1;
				if(chip->r_off == 512)
					chip->r_off = 0;
				printk(KERN_ALERT "off:%d\n",chip->r_off);
		
				chip->r_data[chip->r_in] = read_data;
				printk(KERN_ALERT "interrupt7\n");
				chip->r_in++;
				if(chip->r_in == data_max)
					chip->r_in = 0;
				if(chip->r_in == chip->r_out)
					chip->r_locked = 1;
				count--;
			}
		}else{
			chip->irq_count = count - num;
			while(num){
				read_data = *(chip->io_addr_w+chip->r_off);
				printk(KERN_ALERT "write_data:%04x\n",read_data);
				chip->r_off += 1;
				if(chip->r_off == 512)
					chip->r_off = 0;
				printk(KERN_ALERT "off:%d\n",chip->r_off);
		
				chip->r_data[chip->r_in] = read_data;
				printk(KERN_ALERT "interrupt7\n");
				chip->r_in++;
				if(chip->r_in == data_max)
					chip->r_in = 0;
				if(chip->r_in == chip->r_out)
					chip->r_locked = 1;
				num--;
			}
		}
		up(&chip->rblock);
		wake_up_interruptible(&chip->owq);
	return 1;
	//中断底部 将读取的数据缓存
}

//中断顶部
static irqreturn_t fpga_interrupt(int irq,void *dev_id)
{
	struct platform_device *fpga_dev = dev_id;
	struct fpga *chip = dev_get_drvdata(&fpga_dev->dev);
	uint16_t io_data_r = 0;
	int ret;
	//struct atmel_fpga_data *fpga_data = chip->pfpga->dev.platform_data;
	printk(KERN_ALERT "interrupt1\n");
		spin_lock_irq(&chip->fpga_lock);
		
			io_data_r = (__raw_readl(chip->io_read) & 0xFFFF0) >> 4;
			__raw_writel(0x08000000,chip->piob_sodr);

			*chip->io_addr_r = io_data_r;
			printk(KERN_ALERT "interrupt_read:%04x,addr:%08x\n",*chip->io_addr_r,chip->io_addr_r);
			if((chip->io_addr_r += 1) ==  (chip->io_addr_w + 512 ))
				chip->io_addr_r = chip->io_addr_w;
			__raw_writel(0x08000000,chip->piob_codr);
			printk(KERN_ALERT "read_addr:%08x\n",chip->io_addr_r);
		spin_unlock_irq(&chip->fpga_lock);
	printk(KERN_ALERT "interrupt2\n");
	//schedule_work(&chip->irq_wq);
	ret = queue_work(chip->irq_queue,&chip->irq_wq);
	chip->irq_count++;
	return IRQ_HANDLED;
}

int fpga_open(struct inode *inode,struct file *file)
{
	//打开设备后的初始操作
	struct fpga *chip = container_of(inode->i_cdev,struct fpga,cdev);
	file->private_data = chip;
	return 0;
}

//向FPGA写入数据的内核线程
static void fpga_thread_write(struct fpga *chip){
	printk(KERN_ALERT "right\n");
	while(1){
		//struct atmel_fpga_data *fpga_data = chip->pfpga->dev.platform_data;
		
		printk(KERN_ALERT "right5\n");
		if(down_interruptible(&chip->tblock))
			return -ERESTARTSYS;
		while( (chip->t_in == chip->t_out) && (chip->t_locked == 0) ){
			up(&chip->tblock);
			//printk(KERN_ALERT "waked up 1 t_in:%d,t_out:%d,t_locked:%d\n",chip->t_in,chip->t_out,chip->t_locked);
			if(wait_event_interruptible(chip->oqw,(chip->t_in != chip->t_out)))
				return -ERESTARTSYS;
			
			if(down_interruptible(&chip->tblock))
				return -ERESTARTSYS;
			printk(KERN_ALERT "waked up 2 t_in:%d,t_out:%d,t_locked:%d\n",chip->t_in,chip->t_out,chip->t_locked);
		}
		chip->io_data = chip->t_data[chip->t_out];
		printk(KERN_ALERT "right8\n");
		if((__raw_readl(chip->piob_pdsr) & 0x02000000)){
			spin_lock_irq(&chip->fpga_lock);
			
			__raw_writel(0x000FFFF0,chip->io_clear);
			__raw_writel(chip->io_data << 4,chip->io_write);
			
			__raw_writel(0x04000000,chip->piob_sodr);
			while(!(__raw_readl(chip->piob_pdsr) & 0x00400000));
			__raw_writel(0x04000000,chip->piob_codr);
			
			spin_unlock_irq(&chip->fpga_lock);
			chip->t_out++;
			if(chip->t_out == data_max)
				chip->t_out = 0;
			if(chip->t_out == chip->t_in)
				chip->t_locked = 0;
		}
		up(&chip->tblock);
		wake_up_interruptible(&chip->iqw);
		
		printk(KERN_ALERT "right9\n");
	}
} 

ssize_t fpga_write(struct file *file,const char __user *buf,size_t size, loff_t *offset)
{
	//向FPGA中写入数据
	printk(KERN_ALERT "right1\n");
	struct fpga *chip = file->private_data;
	uint16_t w_data;
	copy_from_user(&w_data,buf,sizeof(uint16_t));
	printk(KERN_ALERT "right2\n");
	if(down_interruptible(&chip->tblock))
		return -ERESTARTSYS;
	while( (chip->t_in == chip->t_out) && (chip->t_locked == 1) ){
		up(&chip->tblock);
		if(wait_event_interruptible(chip->iqw,chip->t_in != chip->t_out))
			return -ERESTARTSYS;
		if(down_interruptible(&chip->tblock))
			return -ERESTARTSYS;
	}
	printk(KERN_ALERT "right3\n");
		chip->t_data[chip->t_in] = w_data;
		chip->t_in++;
		if(chip->t_in == data_max)
			chip->t_in =0;
		if(chip->t_in == chip->t_out)
			chip->t_locked = 1;
	up(&chip->tblock);
	wake_up_interruptible(&chip->oqw);
	printk(KERN_ALERT "right4\n");
	return size; 
} 

ssize_t fpga_read(struct file *file,char __user *buf,size_t size,loff_t *offset )
{
	struct fpga *chip = file->private_data;	
	if(down_interruptible(&chip->rblock))
		return -ERESTARTSYS;
	while( (chip->r_in == chip->r_out) && (chip->r_locked == 0) ){
		up(&chip->rblock);
		if(wait_event_interruptible(chip->owq,chip->r_in != chip->r_out))
			return -ERESTARTSYS;
		if(down_interruptible(&chip->rblock))
			return -ERESTARTSYS;
	}
	copy_to_user(buf,&chip->r_data[chip->r_out],sizeof(uint16_t));
	chip->r_out++;
	if(chip->r_out == data_max)
		chip->r_out = 0;
	if(chip->r_out == chip->r_in)
		chip->r_locked = 0;
	
	up(&chip->rblock);
	wake_up_interruptible(&chip->iwq);
	
	//从FPGA中读取数据
	return size;
}

//驱动的基本操作
static struct file_operations fpga_fops = {
	.owner = THIS_MODULE,
	.open = fpga_open,
	.read = fpga_read,
	.write = fpga_write,
};


//挂载驱动  完成驱动的初始化
static int fpga_probe(struct platform_device *dev)
{
	struct resource *r;
	struct fpga *chip;
	int ret;
	
	printk(KERN_ALERT "yes2 !!\n");
	chip = kmalloc(sizeof(struct fpga),GFP_KERNEL);
	printk(KERN_ALERT "yes10 !!\n");
	if(!chip){
	  printk(KERN_ALERT "error!!\n");
	}
	//方案2
	chip->io_addr_r = kmalloc(1024,GFP_KERNEL);
	chip->io_addr_w = chip->io_addr_r;
	
	//修改_6
	
	chip->io_clear = (void __iomem*)(0xFEF78000) +0x87400 + 0x34;
	chip->io_write = (void __iomem*)(0xFEF78000) +0x87400 + 0x30;
	chip->io_read  = (void __iomem*)(0xFEF78000) +0x87400 + 0x3C;
	
	chip->piob_sodr = (void __iomem*)(0xFEF78000) +0x87600 + 0x30;
	chip->piob_codr = (void __iomem*)(0xFEF78000) +0x87600 + 0x34;
	chip->piob_pdsr = (void __iomem*)(0xFEF78000) +0x87600 + 0x3C;
	
	r = platform_get_resource(dev,IORESOURCE_IRQ,0);
	printk(KERN_ALERT "yes9 !!\n");
	pin = r->start;

	dev_set_drvdata(&dev->dev,chip);
	printk(KERN_ALERT "yes11 !!\n");
	chip->pfpga = dev;
	spin_lock_init(&chip->fpga_lock);
	printk(KERN_ALERT "yes12 !!\n");
	//sema_init(&chip->lock,20);
	init_MUTEX(&chip->rblock);
	init_MUTEX(&chip->tblock);
	printk(KERN_ALERT "yes13 !!\n");
	init_waitqueue_head(&chip->iwq);
	init_waitqueue_head(&chip->owq);
	init_waitqueue_head(&chip->iqw);
	init_waitqueue_head(&chip->oqw);
	printk(KERN_ALERT "yes14 !!\n");
	chip->r_in = 0;
	chip->r_out = 0;
	chip->t_in = 0;
	chip->t_out = 0;
	chip->r_off = 0;
	chip->t_locked = 0;
	chip->r_locked = 0;
	printk(KERN_ALERT "yes15 !!\n");
	chip->irq_queue = create_workqueue("my_queue");
	INIT_WORK(&chip->irq_wq,fpga_do_work);
	chip->irq_count = 0;
	printk(KERN_ALERT "yes16 !!\n");
	ret = request_irq(pin,fpga_interrupt,IRQ_TYPE_EDGE_FALLING ,"myfpga",dev);
	//ret = request_irq(pin,fpga_interrupt,IRQF_DISABLED | IRQF_TRIGGER_FALLING,"myfpga",dev);
	if(ret<0){
	  printk(KERN_ALERT "error\n");	
	}
	printk(KERN_ALERT "yes17 !!\n");
	cdev_init(&chip->cdev,&fpga_fops);
	chip->cdev.owner = THIS_MODULE;
	printk(KERN_ALERT "yes18 !!\n");
	ret = cdev_add(&chip->cdev,fpga_devt,1);	
	if(ret<0){
	  printk(KERN_ALERT "error\n");	
	}
	printk(KERN_ALERT "yes19 !!\n");
	chip->class_dev = device_create(cls,NULL,fpga_devt,&dev->dev,"my_fpga2");	
	if(IS_ERR(chip->class_dev)){
	  printk(KERN_ALERT "error\n");	
	}
	printk(KERN_ALERT "yes20 !!\n");
	fpga_thread = kthread_run(fpga_thread_write,chip,"fpga_write");
	printk(KERN_ALERT "yes21 !!\n");
	return 0;

}


int fpga_remove(struct platform_device *dev)
{
	struct fpga *chip = dev_get_drvdata(&dev->dev);

	device_unregister(chip->class_dev);
	cdev_del(&chip->cdev);
	class_destroy(cls);
	free_irq(pin,dev);
	//iounmap(gpio_con);
	kthread_stop(fpga_thread);
	
	//方案2
	kfree(chip->io_addr_w);
	
	
	kfree(chip);
	return 0;
}

static struct platform_driver fpga_driver = 
{
	.probe = fpga_probe,
	.remove = fpga_remove,
	.driver = {
	  .name = "myfpga",
	  .owner = THIS_MODULE,
	}
};


static int fpga_drv_init(void)
{
	int ret;
	
	cls = class_create(THIS_MODULE, "fpga_class_2");
	if(IS_ERR(cls))
		{
		  return PTR_ERR(cls);
		}
	ret = alloc_chrdev_region(&fpga_devt,0,8,"myfpga");
	if(ret < 0){
	  class_destroy(cls);
	  return ret;
	}
	printk(KERN_ALERT "yes1 !!\n");

	platform_driver_register(&fpga_driver);
	printk(KERN_ALERT "yes22 !!\n");
	return 0;
}

static int fpga_drv_exit(void)
{
	device_destroy(cls,MKDEV(MAJOR(fpga_devt),0));
	class_destroy(cls);
	platform_driver_unregister(&fpga_driver);
	return 0;
}

module_init(fpga_drv_init);
module_exit(fpga_drv_exit);

MODULE_LICENSE("GPL"); 
