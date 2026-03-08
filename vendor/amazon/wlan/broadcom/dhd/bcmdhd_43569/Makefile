
#DHDCFLAGS += -mlittle-endian
#DHDCFLAGS += -fshort-wchar -fno-pic
DHDCFLAGS += -fno-pic
DHDCFLAGS += -Wall -Wstrict-prototypes -Werror
DHDCFLAGS += -DLINUX -Dlinux
DHDCFLAGS += -D__KERNEL__

DHDCFLAGS += -DCONFIG_WLAN_AARCH64_SUPPORT

DHDCFLAGS += -DBCMDRIVER -DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE
DHDCFLAGS += -DSHOW_EVENTS -DBCMDBG
DHDCFLAGS += -DEMBEDDED_PLATFORM
DHDCFLAGS += -DDHD_BCMEVENTS
DHDCFLAGS += -DWIFI_ACT_FRAME
DHDCFLAGS += -DARP_OFFLOAD_SUPPORT
DHDCFLAGS += -DKEEP_ALIVE
DHDCFLAGS += -DCSCAN
DHDCFLAGS += -DPKT_FILTER_SUPPORT
DHDCFLAGS += -DPNO_SUPPORT
DHDCFLAGS += -DSHOW_LOGTRACE
DHDCFLAGS += -DCUSTOMER_HW2
DHDCFLAGS += -DGET_CUSTOM_MAC_ENABLE
DHDCFLAGS += -DDISABLE_SUSPEND_MODE

DHDCFLAGS += -DWLP2P
DHDCFLAGS += -DWL_ENABLE_P2P_IF
DHDCFLAGS += -DPASSWORDLESS_GO_SUPPORT

#For WiFi remote. Keep reporting Probe_Request events ON.
DHDCFLAGS += -DWL_SUPPORT_P2P_AUTO_GO


DHDCFLAGS += -DBDC
DHDCFLAGS += -DTOE
DHDCFLAGS += -DPROP_TXSTATUS

DHDCFLAGS += -DWL_CFG80211
DHDCFLAGS += -DWL_CFG80211_STA_EVENT
DHDCFLAGS += -DWL_IFACE_COMB_NUM_CHANNELS
DHDCFLAGS += -DWL_NAN
DHDCFLAGS += -DWLTDLS
DHDCFLAGS += -DWL_SDO
DHDCFLAGS += -DWL_SCHED_SCAN

DHDCFLAGS += -DDUAL_STA
DHDCFLAGS += -DVSDB
DHDCFLAGS += -DMFP

DHDCFLAGS += -DOEM_ANDROID
DHDCFLAGS += -DBCM_REQUEST_FW

DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DSET_RANDOM_MAC_SOFTAP

DHDCFLAGS += -DUSBOS_TX_THREAD
DHDCFLAGS += -DBCMDHDUSB
DHDCFLAGS += -DBCMDBUS
DHDCFLAGS += -DBCMUSBDEV_COMPOSITE
DHDCFLAGS += -DDBUS_USB_LOOPBACK

DHDCFLAGS += -DBCMTRXV2


DHDCFLAGS += -DDISABLE_BUILTIN_ROAM
DHDCFLAGS += -DDISABLE_11H_SOFTAP
DHDCFLAGS += -DSUPPORT_PM2_ONLY

###################

DHDCFLAGS += -DWL11ULB
DHDCFLAGS += -DWL_VIRTUAL_APSTA
DHDCFLAGS += -DWLAIBSS
DHDCFLAGS += -DWLAIBSS_PS
DHDCFLAGS += -DWL_RELMCAST
DHDCFLAGS += -DBT_WIFI_HANDOVER
DHDCFLAGS += -DWL_SUPPORT_AUTO_CHANNEL
DHDCFLAGS += -DTSQ_MULTIPLIER=10
DHDCFLAGS += -DDISABLE_FRAMEBURST
DHDCFLAGS += -DHT40_GO
DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST

ifeq ($(TARGET_BUILD_VARIANT), user)
DHDCFLAGS += -DCONFIG_NO_PII
endif

ifeq ($(TARGET_PRODUCT), abc123_r2)
DHDCFLAGS += -DMSTAR_ENHANCEMENT
endif

ifeq ($(TARGET_PRODUCT), full_sbc123)
DHDCFLAGS += -DCONFIG_DTS
endif

ifeq ($(TARGET_PRODUCT), csm_sbc123)
DHDCFLAGS += -DCONFIG_DTS
endif

ifeq ($(TARGET_PRODUCT), needle)
DHDCFLAGS += -DENABLE_FASTCONNECT
DHDCFLAGS += -DNEEDLE_SUPPORT_COUNTRY_CODE
endif

ifeq ($(TARGET_PRODUCT), stark)
DHDCFLAGS += -DENABLE_FASTCONNECT
DHDCFLAGS += -DSTARK_SUPPORT_COUNTRY_CODE
endif

#beacon timeout
ifeq ($(TARGET_PRODUCT), csm_sbc123)
DHDCFLAGS += -DCUSTOM_BCN_TIMEOUT=15
endif

ifeq ($(TARGET_PRODUCT), abe123)
DHDCFLAGS += -DCUSTOM_BCN_TIMEOUT=15
endif

ifeq ($(TARGET_PRODUCT), csm_sbc123)
DHDCFLAGS += -DDISABLE_BEAMFORMING
endif

ifneq ($(CONFIG_BCM43569),)
endif

ifneq ($(CONFIG_BCMDHD_USB),)
endif

#DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
CONFIG_DHD_USE_STATIC_BUF=y
# For Static Buffer
ifeq ($(CONFIG_DHD_USE_STATIC_BUF),y)
#  DHDCFLAGS += -DENHANCED_STATIC_BUF
#  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
endif


ifeq (0,1)
$(warning NOTUSED)
endif

DHDCFILES += dhd_linux.c dhd_linux_sched.c linux_osl.c bcmutils.c
DHDCFILES += hnd_pktq.c hnd_pktpool.c bcmevent.c dhd_common.c dhd_ip.c
DHDCFILES += dhd_linux_platdev.c dhd_linux_wq.c bcmxtlv.c dhd_custom_gpio.c
DHDCFILES += dhd_cdc.c dhd_wlfc.c dbus.c dbus_usb.c dbus_usb_linux.c
DHDCFILES += wl_android.c wldev_common.c bcmwifi_channels.c dhd_pno.c
DHDCFILES += wl_cfgnan.c wl_cfg80211.c wl_cfgp2p.c dhd_cfg80211.c
DHDCFILES += wl_cfg_btcoex.c dhd_cfg_vendor.c wl_linux_mon.c
DHDCFILES += dhd_debug.c dhd_debug_linux.c

DHDOFILES += dhd_linux.o dhd_linux_sched.o linux_osl.o bcmutils.o
DHDOFILES += hnd_pktq.o hnd_pktpool.o bcmevent.o dhd_common.o dhd_ip.o
DHDOFILES += dhd_linux_platdev.o dhd_linux_wq.o bcmxtlv.o dhd_custom_gpio.o
DHDOFILES += dhd_cdc.o dhd_wlfc.o dbus.o dbus_usb.o dbus_usb_linux.o
DHDOFILES += wl_android.o wldev_common.o bcmwifi_channels.o dhd_pno.o
DHDOFILES += wl_cfgnan.o wl_cfg80211.o wl_cfgp2p.o dhd_cfg80211.o
DHDOFILES += wl_cfg_btcoex.o dhd_cfg_vendor.o wl_linux_mon.o
DHDOFILES += dhd_debug.o dhd_debug_linux.o


EXTRA_CFLAGS += $(DHDCFLAGS) -DDHD_DEBUG
$(warning CFLAGS=$(EXTRA_CFLAGS))
EXTRA_CFLAGS += -DSRCBASE=\"$(src)\"
EXTRA_CFLAGS += -I$(src)/include/ -I$(src)/
KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(shell pwd)

bcmdhd-objs := $(DHDOFILES)
obj-m   += bcmdhd.o

all:
	@echo "$(MAKE) -C $(LINUXDIR) O=$(BUILD_DIR) M=$(LOCAL_DIR) modules"
	@$(MAKE) -C $(LINUXDIR) O=$(BUILD_DIR) M=$(LOCAL_DIR) V=1 modules

clean:
	rm -rf *.o *.ko *.mod.c *~ .*.cmd *.o.cmd .*.o.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin

move:
	mv $(DHDCFILES) used/

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
