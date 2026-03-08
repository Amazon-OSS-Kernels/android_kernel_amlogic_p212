LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

AML_UBOOT_SRC_PATH := $(LOCAL_PATH)
AML_UBOOT_OUT_PATH := $(abspath $(PRODUCT_OUT)/aml_uboot)

AML_BL2_SRC_DIR := $(LOCAL_PATH)/../../../../vendor/amlogic/common/spl

ifeq ($(TARGET_PRODUCT),needle)
AML_UBOOT_BOARD := needle
else ifeq ($(TARGET_PRODUCT),stark)
AML_UBOOT_BOARD := stark
else ifeq ($(TARGET_PRODUCT),baxter)
AML_UBOOT_BOARD := baxter
else
AML_UBOOT_BOARD := gxl_p212_v1
endif

LOCAL_MODULE := aml_uboot.s905
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := build_aml_uboot

.PHONY: build_aml_uboot
build_aml_uboot: | $(ACP)
	@mkdir -p $(PRODUCT_OUT)/unsigned/ $(AML_UBOOT_OUT_PATH)
	$(MAKE) -C bootable/bootloader/uboot-amlogic/p212 \
		distclean \
		O=$(AML_UBOOT_OUT_PATH)

	$(MAKE) -C bootable/bootloader/uboot-amlogic/p212 \
		KBUILD_VERBOSE=1 \
		O=$(AML_UBOOT_OUT_PATH) \
			$(AML_UBOOT_BOARD)_config
	$(MAKE) -j 1 -C bootable/bootloader/uboot-amlogic/p212 \
		KBUILD_VERBOSE=1 \
		O=$(AML_UBOOT_OUT_PATH)
	$(ACP) $(AML_UBOOT_OUT_PATH)/u-boot.bin $(PRODUCT_OUT)/unsigned/bl33.bin
	# FIXME: This shouldn't be copied out from source path, we need to
	# fix the top-level Makefile eventually
	$(ACP) $(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl21.bin \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/acs.bin \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl301.bin \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/u-boot.bin \
		$(PRODUCT_OUT)/unsigned
	# Assume if spl dir exists, others are also exist
	$(if $(wildcard $(AML_BL2_SRC_DIR)),, \
	    $(ACP) $(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl2.bin \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl30.bin \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl31.img \
		$(AML_UBOOT_SRC_PATH)/fip/gxl/$(AML_UBOOT_BOARD)/bl32.img \
		$(PRODUCT_OUT)/unsigned)
	@echo "Built U-Boot successfully"

include $(BUILD_PHONY_PACKAGE)
