# ZTE Axon M (`fujisan`)

Initial LineageOS 15.1 bring-up tree.

Current scope:
- boot a single-screen system image
- keep WiFi / touch bring-up paths visible in the tree
- defer hinge, dual-screen composition and secondary touch routing

Known input devices on stock Oreo:
- `zte-touchscreen`: primary panel touch
- `zte-touchscreen-2nd`: secondary panel touch
- `zte-touchsrceen-3nd`: misleading helper node for the dual-screen stack

Image signing notes:
- Stock `boot` and `recovery` both contain an appended DER-style signature
  trailer after the padded Android boot image payload.
- Measured trailer sizes from the stock images are roughly `1322` bytes for
  `boot` and `1326` bytes for `recovery`.
- The live device reports `ro.boot.flash.locked=1`,
  `ro.boot.verifiedbootstate=yellow`, and `ro.boot.veritymode=enforcing`.
- For LineageOS bring-up this means `boot.img` must be boot-signed just like
  `recovery.img`. The product makefile therefore inherits `verity.mk` and
  enables the boot signer at the product level, not only inside `device.mk`.
- LineageOS 15.1 / Android 8.1 only auto-enables parts of `verity.mk` for
  `user` and `userdebug`. Since bring-up here uses `eng`, the product makefile
  also forces the required verity / boot-signer product variables explicitly.
- `system.img` is different: it is a sparse ext4 image with no appended boot
  signature trailer. `/system` verification belongs to dm-verity / verified
  boot policy, not the boot-image signer path.

Kernel bring-up notes:
- The default path builds `Image.gz-dtb` from `kernel/zte/fujisan`.
- For fast A/B diagnosis the device tree also supports a prebuilt kernel path.
- To force a prebuilt kernel for `bootimage`, export
  `PREBUILT_KERNEL=true` before running the build.
- The default prebuilt path is
  `device/zte/fujisan/prebuilt-kernel/Image.gz-dtb.stock`.
- To override that file, also export
  `FUJISAN_PREBUILT_KERNEL_PATH=/absolute/path/to/Image.gz-dtb`.
- In prebuilt mode the device tree forces `TARGET_PREBUILT_KERNEL`, clears the
  source-kernel path variables, and errors out if the file does not exist.
  This keeps the current ramdisk and signing flow, and only swaps the kernel
  payload inside `boot.img`.
- This is useful for separating kernel regressions from ramdisk / userspace
  issues when a newer boot image reboots before Android is reachable.

Display CAF patch notes:
- Keep temporary bring-up fixes for `hardware/qcom/display-caf/msm8996` under
  `device/zte/fujisan/patches/display-caf/msm8996/`.
- Apply them from the Lineage source root with:
  `git -C hardware/qcom/display-caf/msm8996 apply --reject --whitespace=nowarn ~/los15.1/device/zte/fujisan/patches/display-caf/msm8996/*.patch`
- `--reject` is intentional here: this tree can drift slightly between sync
  snapshots, and partial hunk failures are easier to inspect than `git am`
  aborting on mismatched blob metadata.
