#
# Copyright 2019-2020 Amazon.com, Inc. or its Affiliates. All rights reserved.
#
LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE_INCLUDES += $(LOCAL_DIR)/../../include
MODULE_INCLUDES += $(LOCAL_DIR)/include

MODULE_CFLAGS += -DUFBL_FEATURE_BCB

MODULE_SRCS += \
    $(LOCAL_DIR)/bcb.c
