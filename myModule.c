#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/fs.h>	//file_operations structure- allow user to open/close/read/write to device
#include<linux/cdev.h>	//help register character device to the kernel
#include<linux/semaphore.h>	//synchronization
#include<asm/uaccess.h>	//copy_to_user and copy_from_user- map data from userspace to kernel space
    
//(1) Create a structure for our device
struct myDevice {
    char data[100];	// represents a fake device
    //char* data;         // represents a fake device
    struct semaphore sem;
}virtual_device;

// variables should be declared as many outside the function/kernel as possible - kernel has a small stack
//(2) cdev object and other variables to register device
struct cdev *mycdev;	// represents character device driver
int major_number;	// major number of the device - extracted from dev_t
int ret;		// return value
dev_t dev_num;		// structure holding major and minor number of the device driver
#define DEVICE_NAME "myDevice"


//(7) implement call back functions
//  inode reference to the file on disk
//  and contains information about that file
//  struct file represents an abstract open file
//  int (*open) (struct inode *, struct file *);
int device_open(struct inode *inode, struct file *filp){
   //semaphore for synchronization - only allow one process to open this device 
   if(down_interruptible(&virtual_device.sem) != 0){    //attempt to accquire given semaphore
        printk(KERN_ALERT "myDriver: could not acquire semaphore for this device\n");
        return -1;  
   }

   printk(KERN_ALERT "myDriver: device opened\n");
   return 0;
}

//(8) called when user do a device reawd (want to read from the device)
//ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
ssize_t device_read(struct file *filp, char* bufStoreData, size_t bufCount, loff_t* curOffset){
    // take data from kernel space(device) to user space(process)
    //copy_to_user(destination, source, sizeToTransfer)
    printk(KERN_INFO "myDriver: user reading from device\n");
    // linux API function
    ret = copy_to_user(bufStoreData, virtual_device.data, bufCount);
    //printk("myDriver: DEVICE BUFFER --> USER SPACE: %s", &virtual_device.data);
    printk("myDriver: DEVICE BUFFER --> USER SPACE: %s", virtual_device.data);
    return ret;
}


//(9) called when user do a device write (wants to send information to the device)
// ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
ssize_t device_write(struct file* filp, const char* buffSourceData,  size_t buffCount, loff_t* curOffset){
    //send data from user to kernel
    //copy_from_user(dest,source,count)
    printk(KERN_INFO "myDriver: user writing to device\n");
    // linux API function
    ret = copy_from_user(virtual_device.data, buffSourceData, buffCount);
    // printk(KERN_INFO "copy_from_user return value: %d: ", ret);
    printk("myDriver: USER SPACE --> DEVICE BUFFER: %s", buffSourceData);
    return ret;
}


//(10) close the device
//int (*release) (struct inode *, struct file *);
int device_close(struct inode *inode, struct file *filp){
    // by calling up(opposite to down for semaphore), we release the semaphore
    // this allows other device to use the device now
    up(&virtual_device.sem);
    printk(KERN_INFO "myDriver: device closed\n");
    printk(KERN_INFO "-----------------------------\n");
    return 0;
}


//(6) tell kernel which functions to call when user operates on device file - call back functions
//struct file_operations {
//    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
//    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
//    int (*open) (struct inode *, struct file *);
//    int (*release) (struct inode *, struct file *);
//};
struct file_operations fops = {
//    .owner = THIS_MODULE,   //prevent unloading of this module when operations are in use
    .open = device_open,    //call this method when opening the device
    .release = device_close,//call this method when closing the device
    .write = device_write,  //call this method when writing to the device
    .read = device_read    //call this method when reading from the device
};


//(3) register the device driver to the systema- two steps
static int setupDriver(void){
    // step_1: allocate our device
    // dynamically allocate device major number and specified minor number
    //ret = alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME);    // register a range of char device numbers
    if(alloc_chrdev_region(&dev_num,0,1,DEVICE_NAME)<0){
	printk(KERN_ALERT "myDriver: failed to allocate a major number\n");
	return ret;
    }

    //major_number = MAJOR(dev_num);
    printk(KERN_INFO "================Loading Driver to Kernel================\n");
    printk(KERN_INFO "           myDriver: device major number is %d\n", MAJOR(dev_num));
    printk(KERN_INFO "     use \"mknod /dev/%s c %d 0\" for device file\n",  DEVICE_NAME, MAJOR(dev_num));
    
    // step_2 create/initialize and add device
    mycdev = cdev_alloc(); //create mycdev structure
    mycdev->ops = &fops;   // structure file_operations
    // mycdev->owner = THIS_MODULE;

    // int cdev_add(struct cdev dev, dev_t num, unsigned int count)
    // add mycdev to the kernel
    //ret = cdev_add(mycdev, dev_num, 1);
    if(cdev_add(mycdev, dev_num, 1)<0){
        printk(KERN_ALERT "myDriver: unable to add the character device driver to kernel\n");
    }

    //(4) Initialize semaphore
    sema_init(&virtual_device.sem, 1); // give sem initial value of 1

    //virtual_device.data = kmalloc(100*sizeof(char), GFP_KERNEL);
    strcpy(virtual_device.data, "default");
    printk(KERN_INFO "====================Loading Complete====================\n");
    return 0;
}

//(5) unregister everything in reverse order
static void releaseDriver(void){
    printk(KERN_INFO "=====================Unloading Driver=====================\n");
    cdev_del(mycdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_ALERT "myDriver: driver unloaded\n");
    printk(KERN_INFO "====================Unloading Complete====================\n");
}


// inform the kernel where to start and stop with our module/driver
module_init(setupDriver);
module_exit(releaseDriver);
