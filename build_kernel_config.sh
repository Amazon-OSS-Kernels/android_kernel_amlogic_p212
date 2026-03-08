################################################################################
#
#  build_kernel_config.sh
#
#  Copyright (c) 2016-2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
################################################################################

KERNEL_SUBPATH="kernel/amlogic/p212"
DEFCONFIG_NAME="needle_defconfig"
TARGET_ARCH="arm64"
MAKE_DTBS=y

# Expected image files are seperated with ":"
KERNEL_IMAGES="arch/arm64/boot/Image:arch/arm64/boot/Image.gz"

################################################################################
# NOTE: You must fill in the following with the path to a copy of an
# aarch64-linux-gnu compiler, i.e gcc-linaro-aarch64-linux-gnu-4.9-2014.09_linux
################################################################################
CROSS_COMPILER_PATH=""
TOOLCHAIN_PREFIX="aarch64-linux-gnu-"
