
function locate_top_dir() {
	curr=$1
	while :
	do
		# The top directory is where the '.repo' sits.
		if [ -e $curr/.repo ]; then
			export TOP_DIR="`readlink -f $curr`"
			break;
		# ... or where the 'build-scripts' and 'kernel' both
		# sits on non repo code model.
		elif [[ -e $curr/build-scripts && -e $curr/kernel ]]; then
			export TOP_DIR="`readlink -f $curr`"
			break;
		elif [ $curr == "`dirname $curr`" ]; then
            echo "not TOP_DIR..."
#			exit 1
		elif [ $curr == "/" ]; then
            echo "Failed to establish TOP_DIR..."
			exit 1
		fi
		curr="`dirname $curr`"
	done
}

current_dir=$(pwd)
locate_top_dir $current_dir


export LOCAL_DIR=${current_dir}
export BUILD_DIR=${current_dir}/build
export TARGETDIR=${TOP_DIR}/out/target/product/abc123/system/lib/modules

###################################################################
# setting the path for cross compiler
###################################################################
export LINUXVER=3.10
export KERNELDIR=${TOP_DIR}/kernel/mediatek/mt8127/3.10
export CROSSTOOL=${TOP_DIR}/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/
export LINUXDIR=${TOP_DIR}/out/target/product/abc123/obj/KERNEL_OBJ
export PATH=${CROSSTOOL}:${LINUXDIR}:$PATH
export CC=arm-eabi-gcc
export STRIP=arm-eabi-strip
export CROSS_COMPILE=arm-eabi-

#These are for compiling under FOS
export TARGET_ARCH=arm
export CFLAGS=-w
#export LINUX_PORT=1

if [ ! -d ${BUILD_DIR} ]; then
mkdir ${BUILD_DIR}
fi

make -f ${LOCAL_DIR}/Makefile


