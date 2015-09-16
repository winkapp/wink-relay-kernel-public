#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>	// wliu

#define TRACE	printk

#define GPIO_WL_REG_ON 25
#define GPIO_WL_HOST_WAKE 9

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17


static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;


static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	printk(KERN_INFO"%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than static size(%d).\n", size, WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

#endif//CONFIG_BROADCOM_WIFI_RESERVED_MEM

static int brcm_wlan_power(int onoff)
{

        printk(KERN_INFO"------------------------------------------------");
        printk(KERN_INFO"------------------------------------------------\n");
        printk(KERN_INFO"%s Enter: power %s\n", __func__, onoff ? "on" : "off");

        if (onoff) {
          gpio_set_value(GPIO_WL_REG_ON, 1);
        } else {
           return 0; //mco-mco  we do not power off the card
           gpio_set_value(GPIO_WL_REG_ON, 0);
        }
        return 0;
}

static int brcm_wlan_reset(int onoff)
{
/// mco-mco     printk("%s ",__FUNCTION__);
	return 0;
}

static int brcm_wifi_cd; /* WIFI virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

int brcm_wifi_status_register(
                void (*callback)(int card_present, void *dev_id),
                void *dev_id)
{
        if (wifi_status_cb)
                return -EAGAIN;
        wifi_status_cb = callback;
        wifi_status_cb_devid = dev_id;
        printk(KERN_INFO "%s: callback is %p, devid is %p\n",
                __func__, wifi_status_cb, dev_id);
        return 0;
}

unsigned int brcm_wifi_status(struct device *dev)
{
/// mco-mco         printk("%s:%d status %d\n",__func__,__LINE__,brcm_wifi_cd);
        return brcm_wifi_cd;
}


static int brcm_wlan_set_carddetect(int val)
{
        TRACE("%s: wifi_status_cb : %p, devid : %p, val : %d\n",
                __func__, wifi_status_cb, wifi_status_cb_devid, val);
        brcm_wifi_cd = val;
        if (wifi_status_cb)
                wifi_status_cb(val, wifi_status_cb_devid);
        else
                pr_warning("%s: Nobody to notify\n", __func__);

        /* msleep(200); wait for carddetect */

        return 0;

}

#define WLC_CNTRY_BUF_SZ        4


struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int custom_locale_rev;
};

struct cntry_locales_custom local_code[] = {{"US","US",46}};

static void *brcm_wlan_get_country_code(char *ccode)
{
    return &local_code[0];
}

/*
static struct resource brcm_wlan_resources[] = {
        [0] = {
                .name   = "bcmdhd_wlan_irq",
                .start  = gpio_to_irq(GPIO_WL_HOST_WAKE),	// wliu	- added gpio_to_irq
                .end    = gpio_to_irq(GPIO_WL_HOST_WAKE),
                .flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
                        | IORESOURCE_IRQ_HIGHLEVEL,
        },
};
 //mco-out */


static struct resource brcm_wlan_resources[] = {
        [0] = {
                .name   = "bcmdhd_wlan_irq",
                .start  = GPIO_WL_HOST_WAKE,
                .end    = GPIO_WL_HOST_WAKE,
                .flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
                        | IORESOURCE_IRQ_HIGHLEVEL,
        },
};


static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif
	.get_country_code = brcm_wlan_get_country_code,
};

static struct platform_device brcm_device_wlan = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(brcm_wlan_resources),
	.resource	= brcm_wlan_resources,
	.dev		= {
		.platform_data = &brcm_wlan_control,
	},
};

int __init brcm_wlan_init(void)
{
	int ret = 0;
	printk(KERN_INFO"%s: start\n", __func__);

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif
	ret = platform_device_register(&brcm_device_wlan);
	return ret;
}
