################################################################################
#
#  build_uboot_config.sh
#
#  Copyright (c) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
################################################################################

UBOOT_SUBPATH="bootable/bootloader/uboot-amlogic/p212"
P212_VARIANT="${P212_VARIANT:-${1:-}}"

if [ -z "$P212_VARIANT" ]; then
  echo "ERROR: Missing P212_VARIANT. Set env var or pass first arg (stark|needle)." >&2
  exit 1
fi

unset TARGET_PRODUCT_NAME_STARK
unset TARGET_PRODUCT_NAME_NEEDLE

case "$P212_VARIANT" in
  stark)
    UBOOT_DEFCONFIG_NAME="stark_defconfig"
    export TARGET_PRODUCT_NAME_STARK=y
    UBOOT_IMAGES="fip/gxl/stark/u-boot.bin"
    ;;
  needle)
    UBOOT_DEFCONFIG_NAME="needle_defconfig"
    export TARGET_PRODUCT_NAME_NEEDLE=y
    UBOOT_IMAGES="fip/gxl/needle/u-boot.bin"
    ;;
  *)
    echo "ERROR: Unsupported P212_VARIANT '$P212_VARIANT' (expected: stark or needle)" >&2
    exit 1
    ;;
esac

# Expected image files are seperated with ":"

################################################################################
# NOTE: You must fill in the following with the path to a copy of an
# gcc-linaro-aarch64-none-elf-4.8-2013.11_linux (aarch64-none-elf compiler) and
# CodeSourcery g++ lite (arm-none-eabi compiler)
################################################################################
export PATH="$PATH:<path/to/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux/bin>"
export PATH="$PATH:<path/to/Sourcery_G++_Lite/bin>"
