ZTE Axon M (`fujisan`) device tree for LineageOS 18.1.

Current bring-up assumptions:
- legacy `/system/vendor` vendor layout with `/vendor` symlink from ramdisk
- 18.1 source rootdir with only legacy-specific ramdisk extras layered on top
- prebuilt kernel is the default path; use `PREBUILT_KERNEL=false` to switch to source-kernel bring-up later
- stock vendor manifest and compatibility matrix are used as the initial VINTF baseline
