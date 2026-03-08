README
======
THESE INSTRUCTIONS AND RELATED BUILD SCRIPTS ARE PROVIDED BY AMAZON ON AN
"AS IS" BASIS. AMAZON MAKES NO REPRESENTATIONS OR WARRANTIES OF ANY KIND,
EXPRESS OR IMPLIED, AS TO THESE INSTRUCTIONS, RELATED BUILD SCRIPTS, OR ANY
THIRD PARTY TECHNOLOGY SUCH AS ANDROID OPEN SOURCE PROJECT CODE OR THIRD PARTY
COMPILERS REFERENCED THEREIN (COLLECTIVELY, “BUILD MATERIALS”). YOU EXPRESSLY
AGREE THAT YOUR USE OF THE BUILD MATERIALS IS AT YOUR SOLE RISK.

AMAZON WILL NOT BE LIABLE FOR ANY DAMAGES OF ANY KIND ARISING FROM THE USE OF
THE BUILD MATERIALS INCLUDING, BUT NOT LIMITED TO, DIRECT, INDIRECT,
INCIDENTAL, PUNITIVE, AND CONSEQUENTIAL DAMAGES.

BUILDING UBoot
-------------------
1.  Obtain a copy of gcc-linaro-aarch64-none-elf-4.8 (aarch64-none-elf compiler)
	or a substitute cross-compiler. Recommended version is 2013.11. Add its bin
	path to environment variable PATH

	export PATH="<path/to/aarch64-none-elf/bin>:$PATH"

2.  Obtain a copy of CodeSourcery g++ lite (arm-none-eabi compiler)
	or a substitute cross-compiler. Recommended version is 2010q1. Add its bin
	path to environment variable PATH

	export PATH="<path/to/arm-none-eabi/bin>:$PATH"

3.  Execute the make script for Amazon Fire TV (3rd Generation) by running:

    ./mk needle

4.  Output can be found in fip/u-boot.bin
