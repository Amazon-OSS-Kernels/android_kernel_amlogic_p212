################################################################################
#
#  build_kernel_config.sh
#
#  Copyright (c) 2016-2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
################################################################################

KERNEL_SUBPATH="kernel/amlogic/p212"
P212_VARIANT="${P212_VARIANT:-${1:-}}"

if [ -z "$P212_VARIANT" ]; then
  echo "ERROR: Missing P212_VARIANT. Set env var or pass first arg (stark|needle)." >&2
  exit 1
fi

case "$P212_VARIANT" in
  stark)
    DEFCONFIG_NAME="stark_defconfig"
    ;;
  needle)
    DEFCONFIG_NAME="needle_defconfig"
    ;;
  *)
    echo "ERROR: Unsupported P212_VARIANT '$P212_VARIANT' (expected: stark or needle)" >&2
    exit 1
    ;;
esac

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
