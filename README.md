ZTE Axon M (`fujisan`) device tree for LineageOS 18.1.

Current bring-up assumptions:
- legacy `/system/vendor` vendor layout with `/vendor` symlink from ramdisk
- 18.1 source rootdir with only legacy-specific ramdisk extras layered on top
- kernel source/config are always declared for Lineage build logic; prebuilt kernel is forced by default, and use `PREBUILT_KERNEL=false` to switch to source-kernel bring-up later
- stock vendor manifest and compatibility matrix are used as the initial VINTF baseline
# Fujisan (ZTE Axon M)

The LineageOS 19.1 adaptation is self-contained in this device tree together
with the regular kernel and vendor trees. It does not require framework,
native, Settings, or Trebuchet source replacements.
