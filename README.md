## Supported Devices

- [Amazon Fire TV (3rd Generation)](https://github.com/Amazon-OSS-Kernels/android_kernel_amlogic_p212/tree/3rd_generation/firetv-3rd-generation-6.2.9.4-20221228)
- [Fire TV Cube (1st Generation)](https://github.com/Amazon-OSS-Kernels/android_kernel_amlogic_p212/tree/cube_1st_generation/firetvcube-6.2.9.4-20221228)

## Unified Build Config

This branch keeps one set of build scripts for both devices:

- `build_kernel_config.sh`
- `build_uboot_config.sh`

Pick the target with either:

- first argument: `stark` or `needle`
- env var: `P212_VARIANT=stark|needle`

No default is used. If you do not pass a variant, the scripts exit with an error.

## Why this branch exists

I compared the latest tips of the two device branches and the meaningful build-script differences were the same `stark` vs `needle` selector values. This branch folds that into one simple selector flow.
