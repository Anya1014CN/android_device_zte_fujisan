ZTE Axon M (`fujisan`) device tree for LineageOS 18.1.

Current bring-up assumptions:
- legacy `/system/vendor` vendor layout with `/vendor` symlink from ramdisk
- 18.1 source rootdir with only legacy-specific ramdisk extras layered on top
- kernel source/config are always declared for Lineage build logic; prebuilt kernel is forced by default, and use `PREBUILT_KERNEL=false` to switch to source-kernel bring-up later
- stock vendor manifest and compatibility matrix are used as the initial VINTF baseline
# Fujisan (ZTE Axon M)

## LineageOS 19.1 upstream replacement

This device has four source replacements in addition to its regular device,
kernel, and vendor trees: `frameworks/base`, `frameworks/native`, Settings, and
Trebuchet. Before syncing a fresh tree, install the local manifest stored at
`upstream-manifest/fujisan.xml` as
`.repo/local_manifests/fujisan.xml`, then sync all four replacement paths.

The matching standard `lineage.dependencies` entry is retained for roomservice
dependency discovery.  Roomservice deliberately does not replace projects
already present in the main Lineage manifest, so the local-manifest override is
necessary to select the `Anya1014CN` fork rather than LineageOS upstream.
