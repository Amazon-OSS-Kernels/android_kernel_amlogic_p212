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

This branch was created by extracting and replaying the meaningful kernel-update changes from the original device branches, then consolidating what overlapped.

In practice:

- kernel import/update commits that are shared between branches were split out and kept as common history
- branch-specific leftovers were separated so they can be reviewed independently
- build-script differences were reduced to the same `stark` vs `needle` selector values

The result is a cleaner branch where shared kernel update content is preserved, and device selection stays in one simple selector flow.
