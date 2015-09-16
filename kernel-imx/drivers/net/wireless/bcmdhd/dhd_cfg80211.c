/*
 * Linux cfg80211 driver - Dongle Host Driver (DHD) related
 *
 * $Copyright Open Broadcom Corporation$
 *
 * $Id: wl_cfg80211.c,v 1.1.4.1.2.14 2011/02/09 01:40:07 Exp $
 */

#include <net/rtnetlink.h>

#include <bcmutils.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <dhd_cfg80211.h>

#ifdef PKT_FILTER_SUPPORT
#include <dngl_stats.h>
#include <dhd.h>
#endif

extern struct wl_priv *wlcfg_drv_priv;

#ifdef PKT_FILTER_SUPPORT
extern uint dhd_pkt_filter_enable;
extern uint dhd_master_mode;
extern void dhd_pktfilter_offload_enable(dhd_pub_t * dhd, char *arg, int enable, int master_mode);
#endif

static int dhd_dongle_up = FALSE;

#include <dngl_stats.h>
#include <dhd.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <dhd_cfg80211.h>

static s32 wl_dongle_up(struct net_device *ndev, u32 up);
static s32 wl_dongle_power(struct net_device *ndev, u32 power_mode);
static s32 wl_dongle_glom(struct net_device *ndev, u32 glom, u32 dongle_align);
static s32 wl_dongle_roam(struct net_device *ndev, u32 roamvar,	u32 bcn_timeout);
static s32 wl_dongle_scantime(struct net_device *ndev, s32 scan_assoc_time, s32 scan_unassoc_time);
static s32 wl_dongle_offload(struct net_device *ndev, s32 arpoe, s32 arp_ol);
static s32 wl_pattern_atoh(s8 *src, s8 *dst);
static s32 wl_dongle_filter(struct net_device *ndev, u32 filter_mode);

/**
 * Function implementations
 */

s32 dhd_cfg80211_init(struct wl_priv *wl)
{
	dhd_dongle_up = FALSE;
	return 0;
}

s32 dhd_cfg80211_deinit(struct wl_priv *wl)
{
	dhd_dongle_up = FALSE;
	return 0;
}

s32 dhd_cfg80211_down(struct wl_priv *wl)
{
	dhd_dongle_up = FALSE;
	return 0;
}

s32 dhd_cfg80211_set_p2p_info(struct wl_priv *wl, int val)
{
	dhd_pub_t *dhd =  (dhd_pub_t *)(wl->pub);
	dhd->op_mode |= val;
	WL_ERR(("Set : op_mode=0x%04x\n", dhd->op_mode));
#ifdef ARP_OFFLOAD_SUPPORT
	if (dhd->arp_version == 1) {
		/* IF P2P is enabled, disable arpoe */
		dhd_arp_offload_set(dhd, 0);
		dhd_arp_offload_enable(dhd, false);
	}
#endif /* ARP_OFFLOAD_SUPPORT */

	return 0;
}

s32 dhd_cfg80211_clean_p2p_info(struct wl_priv *wl)
{
	dhd_pub_t *dhd =  (dhd_pub_t *)(wl->pub);
	dhd->op_mode &= ~(DHD_FLAG_P2P_GC_MODE | DHD_FLAG_P2P_GO_MODE);
	WL_ERR(("Clean : op_mode=0x%04x\n", dhd->op_mode));

#ifdef ARP_OFFLOAD_SUPPORT
	if (dhd->arp_version == 1) {
		/* IF P2P is disabled, enable arpoe back for STA mode. */
		dhd_arp_offload_set(dhd, dhd_arp_mode);
		dhd_arp_offload_enable(dhd, true);
	}
#endif /* ARP_OFFLOAD_SUPPORT */

	return 0;
}

static s32 wl_dongle_up(struct net_device *ndev, u32 up)
{
	s32 err = 0;

	err = wldev_ioctl(ndev, WLC_UP, &up, sizeof(up), true);
	if (unlikely(err)) {
		WL_ERR(("WLC_UP error (%d)\n", err));
	}
	return err;
}
static s32 wl_dongle_power(struct net_device *ndev, u32 power_mode)
{
	s32 err = 0;

	WL_TRACE(("In\n"));
	err = wldev_ioctl(ndev, WLC_SET_PM, &power_mode, sizeof(power_mode), true);
	if (unlikely(err)) {
		WL_ERR(("WLC_SET_PM error (%d)\n", err));
	}
	return err;
}

static s32
wl_dongle_glom(struct net_device *ndev, u32 glom, u32 dongle_align)
{
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];

	s32 err = 0;

	/* Match Host and Dongle rx alignment */
	bcm_mkiovar("bus:txglomalign", (char *)&dongle_align, 4, iovbuf,
		sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (unlikely(err)) {
		WL_ERR(("txglomalign error (%d)\n", err));
		goto dongle_glom_out;
	}
	/* disable glom option per default */
	bcm_mkiovar("bus:txglom", (char *)&glom, 4, iovbuf, sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (unlikely(err)) {
		WL_ERR(("txglom error (%d)\n", err));
		goto dongle_glom_out;
	}
dongle_glom_out:
	return err;
}

static s32
wl_dongle_roam(struct net_device *ndev, u32 roamvar, u32 bcn_timeout)
{
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];

	s32 err = 0;

	/* Setup timeout if Beacons are lost and roam is off to report link down */
	if (roamvar) {
		bcm_mkiovar("bcn_timeout", (char *)&bcn_timeout, 4, iovbuf,
			sizeof(iovbuf));
		err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
		if (unlikely(err)) {
			WL_ERR(("bcn_timeout error (%d)\n", err));
			goto dongle_rom_out;
		}
	}
	/* Enable/Disable built-in roaming to allow supplicant to take care of roaming */
	bcm_mkiovar("roam_off", (char *)&roamvar, 4, iovbuf, sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (unlikely(err)) {
		WL_ERR(("roam_off error (%d)\n", err));
		goto dongle_rom_out;
	}
dongle_rom_out:
	return err;
}

static s32
wl_dongle_scantime(struct net_device *ndev, s32 scan_assoc_time,
	s32 scan_unassoc_time)
{
	s32 err = 0;

	err = wldev_ioctl(ndev, WLC_SET_SCAN_CHANNEL_TIME, &scan_assoc_time,
		sizeof(scan_assoc_time), true);
	if (err) {
		if (err == -EOPNOTSUPP) {
			WL_INFO(("Scan assoc time is not supported\n"));
		} else {
			WL_ERR(("Scan assoc time error (%d)\n", err));
		}
		goto dongle_scantime_out;
	}
	err = wldev_ioctl(ndev, WLC_SET_SCAN_UNASSOC_TIME, &scan_unassoc_time,
		sizeof(scan_unassoc_time), true);
	if (err) {
		if (err == -EOPNOTSUPP) {
			WL_INFO(("Scan unassoc time is not supported\n"));
		} else {
			WL_ERR(("Scan unassoc time error (%d)\n", err));
		}
		goto dongle_scantime_out;
	}

dongle_scantime_out:
	return err;
}

static s32
wl_dongle_offload(struct net_device *ndev, s32 arpoe, s32 arp_ol)
{
	/* Room for "event_msgs" + '\0' + bitvec */
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];

	s32 err = 0;

	/* Set ARP offload */
	bcm_mkiovar("arpoe", (char *)&arpoe, 4, iovbuf, sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (err) {
		if (err == -EOPNOTSUPP)
			WL_INFO(("arpoe is not supported\n"));
		else
			WL_ERR(("arpoe error (%d)\n", err));

		goto dongle_offload_out;
	}
	bcm_mkiovar("arp_ol", (char *)&arp_ol, 4, iovbuf, sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (err) {
		if (err == -EOPNOTSUPP)
			WL_INFO(("arp_ol is not supported\n"));
		else
			WL_ERR(("arp_ol error (%d)\n", err));

		goto dongle_offload_out;
	}

dongle_offload_out:
	return err;
}

static s32 wl_pattern_atoh(s8 *src, s8 *dst)
{
	int i;
	if (strncmp(src, "0x", 2) != 0 && strncmp(src, "0X", 2) != 0) {
		WL_ERR(("Mask invalid format. Needs to start with 0x\n"));
		return -1;
	}
	src = src + 2;		/* Skip past 0x */
	if (strlen(src) % 2 != 0) {
		WL_ERR(("Mask invalid format. Needs to be of even length\n"));
		return -1;
	}
	for (i = 0; *src != '\0'; i++) {
		char num[3];
		strncpy(num, src, 2);
		num[2] = '\0';
		dst[i] = (u8) simple_strtoul(num, NULL, 16);
		src += 2;
	}
	return i;
}

static s32 wl_dongle_filter(struct net_device *ndev, u32 filter_mode)
{
	/* Room for "event_msgs" + '\0' + bitvec */
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];

	const s8 *str;
	struct wl_pkt_filter pkt_filter;
	struct wl_pkt_filter *pkt_filterp;
	s32 buf_len;
	s32 str_len;
	u32 mask_size;
	u32 pattern_size;
	s8 buf[256];
	s32 err = 0;

	/* add a default packet filter pattern */
	str = "pkt_filter_add";
	str_len = strlen(str);
	strncpy(buf, str, str_len);
	buf[str_len] = '\0';
	buf_len = str_len + 1;

	pkt_filterp = (struct wl_pkt_filter *)(buf + str_len + 1);

	/* Parse packet filter id. */
	pkt_filter.id = htod32(100);

	/* Parse filter polarity. */
	pkt_filter.negate_match = htod32(0);

	/* Parse filter type. */
	pkt_filter.type = htod32(0);

	/* Parse pattern filter offset. */
	pkt_filter.u.pattern.offset = htod32(0);

	/* Parse pattern filter mask. */
	mask_size = htod32(wl_pattern_atoh("0xff",
		(char *)pkt_filterp->u.pattern.
		    mask_and_pattern));

	/* Parse pattern filter pattern. */
	pattern_size = htod32(wl_pattern_atoh("0x00",
		(char *)&pkt_filterp->u.pattern.mask_and_pattern[mask_size]));

	if (mask_size != pattern_size) {
		WL_ERR(("Mask and pattern not the same size\n"));
		err = -EINVAL;
		goto dongle_filter_out;
	}

	pkt_filter.u.pattern.size_bytes = mask_size;
	buf_len += WL_PKT_FILTER_FIXED_LEN;
	buf_len += (WL_PKT_FILTER_PATTERN_FIXED_LEN + 2 * mask_size);

	/* Keep-alive attributes are set in local
	 * variable (keep_alive_pkt), and
	 * then memcpy'ed into buffer (keep_alive_pktp) since there is no
	 * guarantee that the buffer is properly aligned.
	 */
	memcpy((char *)pkt_filterp, &pkt_filter,
		WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN);

	err = wldev_ioctl(ndev, WLC_SET_VAR, buf, buf_len, true);
	if (err) {
		if (err == -EOPNOTSUPP) {
			WL_INFO(("filter not supported\n"));
		} else {
			WL_ERR(("filter (%d)\n", err));
		}
		goto dongle_filter_out;
	}

	/* set mode to allow pattern */
	bcm_mkiovar("pkt_filter_mode", (char *)&filter_mode, 4, iovbuf,
		sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (err) {
		if (err == -EOPNOTSUPP) {
			WL_INFO(("filter_mode not supported\n"));
		} else {
			WL_ERR(("filter_mode (%d)\n", err));
		}
		goto dongle_filter_out;
	}

dongle_filter_out:
	return err;
}
s32 dhd_config_dongle(struct wl_priv *wl, bool need_lock)
{
#ifndef DHD_SDALIGN
#define DHD_SDALIGN	32
#endif
	struct net_device *ndev;
	s32 err = 0;

	WL_TRACE(("In\n"));
	if (dhd_dongle_up) {
		WL_ERR(("Dongle is already up\n"));
		return err;
	}

	ndev = wl_to_prmry_ndev(wl);

	if (need_lock)
		rtnl_lock();

	err = wl_dongle_up(ndev, 0);
	if (unlikely(err)) {
		WL_ERR(("wl_dongle_up failed\n"));
		goto default_conf_out;
	}
	err = wl_dongle_power(ndev, PM_FAST);
	if (unlikely(err)) {
		WL_ERR(("wl_dongle_power failed\n"));
		goto default_conf_out;
	}
	err = wl_dongle_glom(ndev, 0, DHD_SDALIGN);
	if (unlikely(err)) {
		WL_ERR(("wl_dongle_glom failed\n"));
		goto default_conf_out;
	}
	err = wl_dongle_roam(ndev, (wl->roam_on ? 0 : 1), 3);
	if (unlikely(err)) {
		WL_ERR(("wl_dongle_roam failed\n"));
		goto default_conf_out;
	}
	wl_dongle_scantime(ndev, 40, 80);
	wl_dongle_offload(ndev, 1, 0xf);
	wl_dongle_filter(ndev, 1);
	dhd_dongle_up = true;

default_conf_out:
	if (need_lock)
		rtnl_unlock();
	return err;

}

#ifdef CONFIG_NL80211_TESTMODE
int dhd_cfg80211_testmode_cmd(struct wiphy *wiphy, void *data, int len)
{
	struct sk_buff *reply;
	struct wl_priv *wl;
	dhd_pub_t *dhd;
	dhd_ioctl_t *ioc = data;
	int err = 0;

	WL_TRACE(("entry: cmd = %d\n", ioc->cmd));
	wl = wiphy_priv(wiphy);
	dhd = wl->pub;

	DHD_OS_WAKE_LOCK(dhd);


	/* currently there is only one wiphy for ifidx 0 */
	err = dhd_ioctl_process(dhd, 0, ioc);
	if (err)
		goto done;

	/* response data is in ioc->buf so return ioc here */
	reply = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(*ioc));
	nla_put(reply, NL80211_ATTR_TESTDATA, sizeof(*ioc), ioc);
	err = cfg80211_testmode_reply(reply);
done:
	DHD_OS_WAKE_UNLOCK(dhd);
	return err;
}
#endif /* CONFIG_NL80211_TESTMODE */

/* TODO: clean up the BT-Coex code, it still have some legacy ioctl/iovar functions */
#define COEX_DHCP

/*
 * get named driver variable to uint register value and return error indication
 * calling example: dev_wlc_intvar_get_reg(dev, "btc_params",66, &reg_value)
 */
static int
dev_wlc_intvar_get_reg(struct net_device *dev, char *name,
	uint reg, int *retval)
{
	union {
		char buf[WLC_IOCTL_SMLEN];
		int val;
	} var;
	int error;

	bcm_mkiovar(name, (char *)(&reg), sizeof(reg),
		(char *)(&var), sizeof(var.buf));
	error = wldev_ioctl(dev, WLC_GET_VAR, (char *)(&var), sizeof(var.buf), false);

	*retval = dtoh32(var.val);
	return (error);
}

/**************************************************************************************
 *  The following are copied from Samsung's driver - to pass compiling
 *  
 */
enum wl_cfg80211_btcoex_status {
	BT_DHCP_IDLE,
	BT_DHCP_START,
	BT_DHCP_OPPR_WIN,
	BT_DHCP_FLAG_FORCE_TIMEOUT
};

static int
dev_wlc_bufvar_set(struct net_device *dev, char *name, char *buf, int len)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
	char ioctlbuf_local[1024];
#else
	static char ioctlbuf_local[1024];
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31) */

	bcm_mkiovar(name, buf, len, ioctlbuf_local, sizeof(ioctlbuf_local));

	return (wldev_ioctl(dev, WLC_SET_VAR, ioctlbuf_local, sizeof(ioctlbuf_local), true));
}

/*
get named driver variable to uint register value and return error indication
calling example: dev_wlc_intvar_set_reg(dev, "btc_params",66, value)
*/
static int
dev_wlc_intvar_set_reg(struct net_device *dev, char *name, char *addr, char * val)
{
	char reg_addr[8];

	memset(reg_addr, 0, sizeof(reg_addr));
	memcpy((char *)&reg_addr[0], (char *)addr, 4);
	memcpy((char *)&reg_addr[4], (char *)val, 4);

	return (dev_wlc_bufvar_set(dev, name, (char *)&reg_addr[0], sizeof(reg_addr)));
}


static bool btcoex_is_sco_active(struct net_device *dev)
{
	int ioc_res = 0;
	bool res = FALSE;
	int sco_id_cnt = 0;
	int param27;
	int i;

	for (i = 0; i < 12; i++) {

		ioc_res = dev_wlc_intvar_get_reg(dev, "btc_params", 27, &param27);

		WL_TRACE(("%s, sample[%d], btc params: 27:%x\n",
			__FUNCTION__, i, param27));

		if (ioc_res < 0) {
			WL_ERR(("%s ioc read btc params error\n", __FUNCTION__));
			break;
		}

		if ((param27 & 0x6) == 2) { /* count both sco & esco  */
			sco_id_cnt++;
		}

		if (sco_id_cnt > 2) {
			WL_TRACE(("%s, sco/esco detected, pkt id_cnt:%d  samples:%d\n",
				__FUNCTION__, sco_id_cnt, i));
			res = TRUE;
			break;
		}

		msleep(5);
	}

	return res;
}

int wl_cfg80211_set_btcoex_dhcp(struct net_device *dev, char *command)
{

	struct wl_priv *wl = wlcfg_drv_priv;
	char powermode_val = 0;
	char buf_reg66va_dhcp_on[8] = { 66, 00, 00, 00, 0x10, 0x27, 0x00, 0x00 };
	char buf_reg41va_dhcp_on[8] = { 41, 00, 00, 00, 0x33, 0x00, 0x00, 0x00 };
	char buf_reg68va_dhcp_on[8] = { 68, 00, 00, 00, 0x90, 0x01, 0x00, 0x00 };

	uint32 regaddr;
	static uint32 saved_reg66;
	static uint32 saved_reg41;
	static uint32 saved_reg68;
	static bool saved_status = FALSE;

#ifdef COEX_DHCP
	char buf_flag7_default[8] =   { 7, 00, 00, 00, 0x0, 0x00, 0x00, 0x00};
	struct btcoex_info *btco_inf = wl->btcoex_info;
#endif /* COEX_DHCP */

#ifdef PKT_FILTER_SUPPORT
	dhd_pub_t *dhd =  (dhd_pub_t *)(wl->pub);
#endif

	WL_ERR(("%s enter\n", __FUNCTION__));
	/* Figure out powermode 1 or o command */
	strncpy((char *)&powermode_val, command + strlen("BTCOEXMODE") +1, 1);

	if (strnicmp((char *)&powermode_val, "1", strlen("1")) == 0) {
		WL_TRACE_HW4(("%s: DHCP session starts\n", __FUNCTION__));

#ifdef PKT_FILTER_SUPPORT
		dhd->dhcp_in_progress = 1;

		if (dhd->early_suspended) {
			WL_TRACE_HW4(("DHCP in progressing , disable packet filter!!!\n"));
			dhd_enable_packet_filter(0, dhd);
		}
#endif
		/* Retrieve and saved orig regs value */
		if ((saved_status == FALSE) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 66,  &saved_reg66)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 41,  &saved_reg41)) &&
			(!dev_wlc_intvar_get_reg(dev, "btc_params", 68,  &saved_reg68)))   {
				saved_status = TRUE;
				WL_TRACE(("Saved 0x%x 0x%x 0x%x\n",
					saved_reg66, saved_reg41, saved_reg68));

				/* Disable PM mode during dhpc session */

				/* Disable PM mode during dhpc session */
#ifdef COEX_DHCP
				/* Start  BT timer only for SCO connection */
				if (btcoex_is_sco_active(dev)) {
					/* btc_params 66 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg66va_dhcp_on[0],
						sizeof(buf_reg66va_dhcp_on));
					/* btc_params 41 0x33 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg41va_dhcp_on[0],
						sizeof(buf_reg41va_dhcp_on));
					/* btc_params 68 0x190 */
					dev_wlc_bufvar_set(dev, "btc_params",
						(char *)&buf_reg68va_dhcp_on[0],
						sizeof(buf_reg68va_dhcp_on));
					saved_status = TRUE;

					btco_inf->bt_state = BT_DHCP_START;
					btco_inf->timer_on = 1;
					mod_timer(&btco_inf->timer, btco_inf->timer.expires);
					WL_TRACE(("%s enable BT DHCP Timer\n",
					__FUNCTION__));
				}
#endif /* COEX_DHCP */
		}
		else if (saved_status == TRUE) {
			WL_ERR(("%s was called w/o DHCP OFF. Continue\n", __FUNCTION__));
		}
	}
	else if (strnicmp((char *)&powermode_val, "2", strlen("2")) == 0) {

#ifdef PKT_FILTER_SUPPORT
		dhd->dhcp_in_progress = 0;
		WL_TRACE_HW4(("%s: DHCP is complete \n", __FUNCTION__));

		/* Enable packet filtering */
		if (dhd->early_suspended) {
			WL_TRACE_HW4(("DHCP is complete , enable packet filter!!!\n"));
			dhd_enable_packet_filter(1, dhd);
		}
#endif

		/* Restoring PM mode */

#ifdef COEX_DHCP
		/* Stop any bt timer because DHCP session is done */
		WL_TRACE(("%s disable BT DHCP Timer\n", __FUNCTION__));
		if (btcoex_is_sco_active(dev)) {	// wliu ADDED!
			if (btco_inf->timer_on) {
				btco_inf->timer_on = 0;
				del_timer_sync(&btco_inf->timer);

				if (btco_inf->bt_state != BT_DHCP_IDLE) {
				/* need to restore original btc flags & extra btc params */
					WL_TRACE(("%s bt->bt_state:%d\n",
						__FUNCTION__, btco_inf->bt_state));
					/* wake up btcoex thread to restore btlags+params  */
					schedule_work(&btco_inf->work);
				}
			}
		}

		/* Restoring btc_flag paramter anyway */
		if (saved_status == TRUE) {
			dev_wlc_bufvar_set(dev, "btc_flags",
				(char *)&buf_flag7_default[0], sizeof(buf_flag7_default));
		}
#endif /* COEX_DHCP */

		/* Restore original values */
		if (saved_status == TRUE) {
			regaddr = 66;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg66);
			regaddr = 41;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg41);
			regaddr = 68;
			dev_wlc_intvar_set_reg(dev, "btc_params",
				(char *)&regaddr, (char *)&saved_reg68);

			WL_TRACE(("restore regs {66,41,68} <- 0x%x 0x%x 0x%x\n",
				saved_reg66, saved_reg41, saved_reg68));
		}
		saved_status = FALSE;

	}
	else {
		WL_ERR(("%s Unkwown yet power setting, ignored\n",
			__FUNCTION__));
	}

	snprintf(command, 3, "OK");

	return (strlen("OK"));
}
