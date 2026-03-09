#
# Copyright (c) 2016 - 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
#

# Include rules for each feature
FEATURE_DIR := $(GET_LOCAL_DIR)

ifeq (true,$(strip $(FEATURE_BCB)))
include $(FEATURE_DIR)/bcb/rules_lk2.mk
endif
