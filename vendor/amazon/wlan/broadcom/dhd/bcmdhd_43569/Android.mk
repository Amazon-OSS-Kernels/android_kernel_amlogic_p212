# Copyright Statement:
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

local_path_full := $(shell pwd)/$(LOCAL_PATH)
wifi_module_out_path := $(ANDROID_PRODUCT_OUT)$(WIFI_DRIVER_MODULE_PATH)
wifi_module_target := $(wifi_module_out_path)/bcmdhd.ko

LOCAL_MODULE := bcmdhd_43569
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(wifi_module_target)

include $(BUILD_PHONY_PACKAGE)

$(LOCAL_ADDITIONAL_DEPENDENCIES): PRIVATE_BCMDHD_LOCAL_DIR := $(local_path_full)
$(LOCAL_ADDITIONAL_DEPENDENCIES): PRIVATE_MODULE_OUT := $(wifi_module_out_path)
$(LOCAL_ADDITIONAL_DEPENDENCIES): $(INSTALLED_KERNEL_TARGET) | $(KERNEL_OUT) $(ACP)
	$(hide) rm -rf $(ANDROID_PRODUCT_OUT)$(WIFI_DRIVER_MODULE_PATH)
	$(MAKE) -C $(KERNEL_OUT) M=$(PRIVATE_BCMDHD_LOCAL_DIR) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) modules
	$(hide) cp -f $(PRIVATE_BCMDHD_LOCAL_DIR)/bcmdhd.ko $(PRIVATE_MODULE_OUT)
	$(MAKE) -C $(KERNEL_OUT) M=$(PRIVATE_BCMDHD_LOCAL_DIR) ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) clean

local_path_full :=
wifi_module_out_path :=
wifi_module_target :=
