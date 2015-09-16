# bcmdhd for INTEL 4334, 4335, 43241

DHDCFLAGS += -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER              \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE          \
	-DDHDTHREAD -DDHD_DEBUG -DBDC                                       \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DBCMDBG              		    \
	-DCUSTOMER_HW2 -DOOB_INTR_ONLY -DHW_OOB                             \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P   \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                              \
	-DKEEP_ALIVE -DGET_CUSTOM_MAC_ENABLE -DPKT_FILTER_SUPPORT           \
	-DEMBEDDED_PLATFORM -DENABLE_INSMOD_NO_FW_LOAD -DPNO_SUPPORT        \
	-DDHD_USE_IDLECOUNT -DSET_RANDOM_MAC_SOFTAP -DROAM_ENABLE    	    \
	-DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT       		    \
	-DSUPPORT_PM2_ONLY -DMIRACAST_AMPDU_SIZE=8                          \
	-Idrivers/net/wireless/bcmdhd -Idrivers/net/wireless/bcmdhd/include \
	-Idrivers/net/wireless/bcmdhd/common/include

DHDOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o   \
	dhd_pno.o dhd_linux_sched.o dhd_sdio.o bcmwifi_channels.o bcmevent.o hndpmu.o   \
	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o dhd_wlfc.o dhd_ip.o linux_osl.o \
	bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wldev_common.o wl_android.o

obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)
ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT -DUSE_IW
endif
ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += wl_cfg80211.o wl_cfgp2p.o wl_linux_mon.o dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211 -DWL_CFG80211_STA_EVENT
# WL_ENABLE_P2P_IF and WL_CFG80211_P2P_DEV_IF are mutually exclusive.
# Define WL_CFG80211_P2P_DEV_IF if kernel version is greater than 3.8.0
DFLAGS += -DWL_ENABLE_P2P_IF
#DFLAGS += -DWL_CFG80211_P2P_DEV_IF
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-65
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=15
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=28000
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=7
DHDCFLAGS += -DVSDB -DPROP_TXSTATUS -DPROP_TXSTATUS_VSDB
DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DWL_CFG80211_GON_COLLISION
DHDCFLAGS += -DWLTDLS
DHDCFLAGS += -DWL11U
endif
ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif


################################
# INTEL platform specific features
#################################
ifneq ($(CONFIG_INTEL_PLATFORM),)
  DHDCFLAGS :=$(filter-out -DHW_OOB,$(DHDCFLAGS))
  DHDCFLAGS += -DSUPPORT_AUTO_CHANNEL
  DHDCFLAGS += -DSUPPORT_MULTIPLE_REVISION -DSUPPORT_MULTIPLE_CHIPS
  DHDCFLAGS += -DCUSTOM_LISTEN_INTERVAL=3
  DHDCFLAGS += -DCUSTOM_RXCHAIN=1
endif


#########################
# Chip dependent feature
#########################

ifneq ($(CONFIG_BCM4334),)
$(info bcmdhd: Compiling for 4334 chip)
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=5
endif

ifneq ($(CONFIG_BCM43241),)
$(info bcmdhd: Compiling for 43241 chip)
  DHDCFLAGS += -DUSE_SDIOFIFO_IOVAR
endif

ifneq ($(CONFIG_BCM4335),)
$(info bcmdhd: Compiling for 4335 chip)
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=8
  DHDCFLAGS += -DBCMSDIOH_TXGLOM -DCUSTOM_TXGLOM=1 -DBCMSDIOH_TXGLOM_HIGHSPEED
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
  DHDCFLAGS += -DUSE_SDIOFIFO_IOVAR
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=40
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DCUSTOM_MAX_TXGLOM_SIZE=32
  DHDCFLAGS += -DUSE_DYNAMIC_MAXPKT_RXGLOM
# For BT LOCK
ifeq ($(CONFIG_BCM4335BT),y)
  DHDCFLAGS += -DENABLE_4335BT_WAR
endif
endif
