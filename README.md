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
