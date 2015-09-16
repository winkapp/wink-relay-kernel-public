#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include "gpiopin.h"

//
// http://dokuwiki.brl.flextronics.com/dokuwiki/doku.php?id=udoo-sensors-connection-shematic
// http://www.cs.fsu.edu/~baker/devices/lxr/http/source/linux/drivers/gpio/xilinx_gpio.c
//-----------------------------------------------------------------------------

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marius C @ Flextronics/Burington ON");	/* Who wrote this module? */
MODULE_DESCRIPTION("GPIOPIN driver for udoo board");	/* What does this module do */
MODULE_SUPPORTED_DEVICE("GPIO PIN Access Driver");


#define GPIO1_BASE_OFFF			(0x4000)
/*
struct GpPin
{
    int     port;
    int     value;
    int     a;
    int     b;
};
*/

//-----------------------------------------------------------------------------
static void  __iomem*   _gpmem[7]={0,0,0,0,0,0,0};
static dev_t            _first;
static struct cdev      c_dev;
static struct class*    _cl;
static struct timeval   _prev_tv;
static int              debug=0;

//-----------------------------------------------------------------------------
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "allows char commands read/write bes.");

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
static int check_gnum(int gpio)
{
    int pin;
    int pins[] = {105,125,133,138,143,150,16,164,19, 21,35,47,57,88,
                  0,117,126,134,139,144,156,160,165,192,3, 37,51,89,          // 63 taken out couse hangs
                  1,122,127,135,140,145,157,161,17, 20, 32,40,54,7, 9,
                  101,123,128,136,141,146,158,162,171,203,33,41,55,8, 92,
                  104,124,13,137,142,15,159,163,18, 205,34,42,56,85,-1
                 };
    for(pin=0; pins[pin]!=-1; ++pin)
    {
        if(gpio==pins[pin])
            return 1;
    }
    printk(KERN_WARNING "GPIOPIN:: gpio port: '%d' is not available", gpio);
    return 0;
}


//-----------------------------------------------------------------------------
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}

//-----------------------------------------------------------------------------
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}

//-----------------------------------------------------------------------------
// 222,direction/value,0
static ssize_t my_write(struct file *f,
                        const char __user *buf,
                        size_t len,
                        loff_t *off)
{
    int         *pmbase;
    int         mask,r;
    long        gpio,value;
    char        local[32]= {0};
    char*       dir;
    char*       val;

    r=copy_from_user(local,buf,15);
    if(strchr(local,'\n'))
        *strchr(local,'\n')=0;
    if(strchr(local,'\r'))
        *strchr(local,'\r')=0;

    if(!strchr(local,','))
    {
        printk("GPIOPIN: invalid string format: echo PORT,direction|value,NUMERIC");
        return len;
    }

    dir = strchr(local,',');
    if(!strchr(dir,','))
    {
        printk("GPIOPIN: invalid string format: echo PORT,direction|value,NUMERIC");
        return len;
    }

    val = strchr(dir+1,',');
    *dir++=0;
    *val++=0;

    r=_kstrtol(local,10,&gpio);
    r=_kstrtol(val,10,&value);


    if(check_gnum(gpio)==0)
    {
        return 0;
    }
    if(debug)
        printk("GPIOPIN: write to gpio:%ld,%s,%ld \n",gpio,dir,value);

    mask  = 1 << (gpio % 32);
    if(!strcmp(dir,"direction"))
    {
        if(debug)
            printk("GPIOPIN: direction for: %ld as %ld\n", gpio, value);
        pmbase = (int*)_gpmem[gpio>>5] + 1;
    }
    else
    {
        if(debug)
            printk("GPIOPIN: value for: %ld as %ld\n", gpio, value);
        pmbase = (int*)_gpmem[gpio>>5];
    }
    if(value)
    {
        *pmbase |= mask;
    }
    else
    {
        *pmbase &= ~mask;
    }
    return len;
}

//-----------------------------------------------------------------------------
static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    int* val;
    int  roff = 0,block;

    for(block=0; block<7 && roff < len; block++)
    {
        val = (int*)_gpmem[block];
        roff += sprintf(&buf[roff],"%d:%08X-%08X\n",block,*(val),*(val+1));
    }
    do_gettimeofday(&_prev_tv);
    return roff;
}

#define BIT_VAL() ((*value & bm)!=0)

static int _detect_sync(int* value, int bm)
{
    int reads = 0;
    int vmax = 3000;

    //rezidual value
    while(BIT_VAL()!=0 && vmax-->0)
        udelay(5);

    //start bit
    while(BIT_VAL()==0 && vmax-->0)
        udelay(10);

    // bit value
    while(BIT_VAL() && vmax-->0)
    {
        udelay(3);
        ++reads;

    }
    //printk("[%d] = %d\n",_reads++,reads);
    return reads;
}


static int _steady_on_level(int* value, int bm, int level)
{
    int vmax = 3000;

    //rezidual is any
    while(BIT_VAL() != level)
    {
        udelay(5);
        if(--vmax<0)
            break;
    }
    //s tay here
    while(BIT_VAL()==level)
    {
        udelay(5);
        if(--vmax<0)
            break;
    }
    return vmax>0;
}


static void  _read_vals(int* pmv, int* temp, int* rh, int bm)
{
    int             buf,bit;
    unsigned int    checksum;
    unsigned char   buffer[5];
    unsigned char   byte;


    for(buf=0; buf<5; buf++)
    {
        byte = 0;
        for(bit=0; bit<8; ++bit)
        {
            byte <<=1;
            byte |= _detect_sync(pmv, bm)>14 ? 1 : 0;
        }
        buffer[buf]=byte;
    }

    checksum = 0;
    for (buf = 0 ; buf < 4 ; ++buf)
    {
        checksum += buffer [buf] ;
    }
    checksum&=0xFF;
    //printk("checksum=%d vs byte5=%d\n", checksum, buffer[4]);
    *temp =  ((buffer [2]<<8) + buffer [3]);
    if(*temp & 0x8000)
    {
        *temp &= ~0x8000;
        *temp = -*temp;
    }
    *rh = ((buffer [0]<<8) + buffer [1]);
}



//-----------------------------------------------------------------------------
static long my_ioctl(struct file *file, unsigned int cmd_in, unsigned long arg)
{
    struct  _GpPin   gpio;
    int     *pmchipD,*pmchipV;
    int     mask,r;

    r=copy_from_user(&gpio, (unsigned char*)arg, sizeof(struct _GpPin));
    if(check_gnum(gpio.port)==0)
    {
        return -EINVAL;
    }
    pmchipV = (int*)_gpmem[gpio.port>>5];
    pmchipD = pmchipV+1;
    mask    = 1 << (gpio.port % 32);

    if(debug)
        printk("GPIOPIN: ioctl:[%d] gpio:%d value:%d\n", cmd_in, gpio.port, gpio.value);

    switch(cmd_in)
    {
    case IOCTL_GET_DIRECTION:
        gpio.value =  (*pmchipD & mask) ? 1 : 0;
        r=copy_to_user((unsigned char*)arg, &gpio, sizeof(struct  _GpPin));
        return 0;

    case IOCTL_SET_DIRECTION:
        if(gpio.value)
        {
            *pmchipD |= mask;
        }
        else
        {
            *pmchipD &= ~mask;
        }
        return 0;

    case IOCTL_GET_VALUE:
        gpio.value =  (*pmchipV & mask) ? 1 : 0;
        r=copy_to_user((unsigned char*)arg, &gpio, sizeof(struct _GpPin));
        return 0;
    case IOCTL_SET_VALUE:
        if(gpio.value)
        {
            *pmchipV |= mask;
        }
        else
        {
            *pmchipV &= ~mask;
        }
        return 0;
    case IOCTL_READ_RTH03:
    {
        *pmchipD |= mask;   //  out
        mdelay(3000);       // 2 sec
        *pmchipV &= ~mask;  // 0
        mdelay(2);
        *pmchipV |= mask;  // 1
        udelay(20);
        *pmchipD |= mask;   //  in
        if(_steady_on_level(pmchipV, mask, 0) &&
           _steady_on_level(pmchipV, mask, 1))
        {
            _read_vals(pmchipV, &gpio.a, &gpio.b, mask);
            r=copy_to_user((unsigned char*)arg, &gpio, sizeof(struct _GpPin));
            return 0;
        }
    }
        break;
    default:
        break;
    }
    return -ENOTTY;
}


//-----------------------------------------------------------------------------
static void io_unmap(void)
{
    int block;
    for(block=0; block<7; block++)
    {
        if(_gpmem[block])
            iounmap(_gpmem[block]);
        _gpmem[block]=0;
    }

}

//-----------------------------------------------------------------------------
static struct file_operations _gpmem_fops =
{
    .owner      = THIS_MODULE,
    .open       = &my_open,
    .release    = &my_close,
    .read       = &my_read,
    .write      = &my_write,
    .unlocked_ioctl = &my_ioctl,
};

//-----------------------------------------------------------------------------
static int __init bulkgpio_init(void)
{
    int block;
    int gpio_base_bank;

    for(block=0; block<7; block++)
    {
        gpio_base_bank = GPIO1_BASE_ADDR + (block * GPIO1_BASE_OFFF);
        if ((_gpmem[block] = ioremap(gpio_base_bank, 16)) == NULL)
        {
            printk(KERN_ERR "GPIOPIN:: Mapping gpio RAM failed\n");
            io_unmap();
            return -1;
        }
    }

    if (alloc_chrdev_region(&_first, 0, 1, "gpiopin") < 0)
    {
        printk(KERN_ERR "GPIOPIN:: cannot alloc char region\n");
        io_unmap();
        return -1;
    }
    if ((_cl = class_create(THIS_MODULE, "chardrv")) == NULL)
    {
        printk(KERN_ERR "GPIOPIN:: cannot create class dev\n");
        unregister_chrdev_region(_first, 1);
        io_unmap();
        return -1;
    }
    if (device_create(_cl, NULL, _first, NULL, "gpiopin") == NULL)
    {
        printk(KERN_ERR "GPIOPIN:: cannot create device gpiopin\n");
        class_destroy(_cl);
        unregister_chrdev_region(_first, 1);
        io_unmap();
        return -1;
    }

    cdev_init(&c_dev, &_gpmem_fops);
    if (cdev_add(&c_dev, _first, 1) == -1)
    {
        printk(KERN_ERR "GPIOPIN:: cannot add device gpiopin\n");
        device_destroy(_cl, _first);
        class_destroy(_cl);
        unregister_chrdev_region(_first, 1);
        io_unmap();
        return -1;
    }
    do_gettimeofday(&_prev_tv);
    return 0;
}

//-----------------------------------------------------------------------------
static void __exit bulkgpio_clean(void)
{
    cdev_del(&c_dev);
    device_destroy(_cl, _first);
    class_destroy(_cl);
    unregister_chrdev_region(_first, 1);
    io_unmap();
}


//-----------------------------------------------------------------------------
module_init(bulkgpio_init);
module_exit(bulkgpio_clean);
