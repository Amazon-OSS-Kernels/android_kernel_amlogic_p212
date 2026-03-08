# Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates. All Rights Reserved

#
# Common list of objects files we want to include
#
BCB_OBJECTS += \
    bcb.o \
    cmd_bcb.o

BCB_CFLAGS += -DSUPPORT_UBOOT -DUFBL_FEATURE_BCB

BCB_HEADERS += \
    -I$(UFBL_PATH)/include

# Begin rules for kbuild U-Boot
ifeq (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))

ccflags-y += $(BCB_CFLAGS) $(BCB_HEADERS)
obj-y += $(BCB_OBJECTS)

else # default rules

include $(TOPDIR)/config.mk

BCB_LIB = $(obj)libbcb.o

CFLAGS += \
    $(BCB_CFLAGS) \
    $(BCB_HEADERS)

COBJS  := $(BCB_OBJECTS)
SRCS   := $(COBJS:.o=.c)
OBJS   := $(addprefix $(obj),$(COBJS))

$(BCB_LIB): $(OBJS)
	$(call cmd_link_o_target, $(OBJS))

endif # (yes,$(strip $(UFBL_UBOOT_IS_KBUILD)))
