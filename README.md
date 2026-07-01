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

SELinux bring-up notes:
- The first LOS system/vendor image reached early userspace but left repeated
  `execute_no_trans` denials in pstore for stock vendor daemons such as
  `qseecomd`, `rmt_storage`, `pm-service`, `mm-pp-dpps`, and several HAL
  services. Those files were labeled as generic `vendor_file` in the generated
  system image instead of their stock exec types.
- Keep `androidboot.selinux=permissive` in the boot cmdline while the stock
  proprietary service labels are moved into source policy. This is a temporary
  bring-up switch so the next build can expose missing blobs / linker failures
  after init gets past the first SELinux wall.
- The next pstore pass showed `android.hardware.keymaster@3.0-impl.so` failing
  to dlopen `libkeymaster_staging.so`. Stock keeps that library under
  `/system/lib64`, but the vendor keymaster HAL needs a copy in
  `/system/vendor/lib64` for the vendor linker namespace.

Display CAF patch notes:
- Keep temporary bring-up fixes for `hardware/qcom/display-caf/msm8996` under
  `device/zte/fujisan/patches/display-caf/msm8996/`.
- Apply the current qdMetaData compatibility fix from the Lineage source root
- Apply the current display-caf compatibility fixes from the Lineage source root
  with:
  `sh ~/los15.1/device/zte/fujisan/patches/display-caf/msm8996/apply.sh ~/los15.1`
- This uses exact string replacements instead of `git am` / `git apply`
  because this public bring-up tree can drift across sync snapshots while still
  keeping the same source semantics.
- Current scripted fixes:
  remove invalid `private_handle_t::id` logging from `libqdutils/qdMetaData.cpp`
  and link `libqdutils` into `libgralloc` so `CalcFps` resolves at link time.
