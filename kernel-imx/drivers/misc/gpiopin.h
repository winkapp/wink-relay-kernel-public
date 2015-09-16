#ifndef _GPIO_XPPIN_H_
#define _GPIO_XPPIN_H_

#include <linux/ioctl.h>


#define GPIO_PIN_MAJOR_NUM 10

typedef struct _GpPin
{
    int     port;
    int     value;
    int     a;
    int     b;
}GpPin;


#define DIRECTION_OUT 1
#define DIRECTION_IN  0


#define IOCTL_GET_DIRECTION _IOWR(GPIO_PIN_MAJOR_NUM, 0,   GpPin*)
#define IOCTL_SET_DIRECTION _IOWR(GPIO_PIN_MAJOR_NUM, 1,   GpPin*)
#define IOCTL_GET_VALUE _IOWR(GPIO_PIN_MAJOR_NUM, 2,   GpPin*)
#define IOCTL_SET_VALUE _IOWR(GPIO_PIN_MAJOR_NUM, 3,   GpPin*)
#define IOCTL_READ_RTH03 _IOR(GPIO_PIN_MAJOR_NUM, 4, GpPin*)





#endif
