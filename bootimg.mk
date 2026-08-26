# Fujisan's legacy bootloader verifies the AOSP BootSignature DER trailer on
# boot and recovery images. The matching public AOSP verity test certificate
# is the certificate used by the previously accepted device image.

fujisan_boot_signer := $(HOST_OUT_EXECUTABLES)/boot_signer
fujisan_boot_signing_key := $(DEVICE_PATH)/keys/verity.pk8
fujisan_boot_signing_cert := $(DEVICE_PATH)/keys/verity.x509.pem

fujisan_recovery_kernel := $(PRODUCT_OUT)/kernel-recovery
fujisan_recovery_kernel_patcher := $(DEVICE_PATH)/releasetools/make_recovery_kernel.py
fujisan_recovery_dtb_dir := $(DEVICE_PATH)/prebuilt-kernel/recovery-dtbs

# Use the known-working TWRP DTBs only in recovery. They expose the native
# panel-A framebuffer; the freshly built Linux Image.gz remains unchanged.
$(fujisan_recovery_kernel): $(firstword $(INSTALLED_KERNEL_TARGET)) $(fujisan_recovery_kernel_patcher)
	$(hide) python3 $(fujisan_recovery_kernel_patcher) --input $< --output $@ --dtb-dir $(fujisan_recovery_dtb_dir)


$(foreach b,$(INSTALLED_BOOTIMAGE_TARGET), \
  $(eval $(call add-dependency,$(b),$(call bootimage-to-kernel,$(b)))))

$(INSTALLED_BOOTIMAGE_TARGET): $(MKBOOTIMG) $(fujisan_boot_signer) \
    $(INTERNAL_BOOTIMAGE_FILES) $(fujisan_boot_signing_key) \
    $(fujisan_boot_signing_cert)

	$(call pretty,"Target boot image: $@")
	$(MKBOOTIMG) --kernel $(call bootimage-to-kernel,$@) \
	    $(INTERNAL_BOOTIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) \
	    $(BOARD_MKBOOTIMG_ARGS) --output $@.unsigned
	$(fujisan_boot_signer) /boot $@.unsigned $(fujisan_boot_signing_key) \
	    $(fujisan_boot_signing_cert) $@
	$(hide) rm -f $@.unsigned
	$(call assert-max-image-size,$@,$(call get-bootimage-partition-size,$@,boot))

$(INSTALLED_RECOVERYIMAGE_TARGET): $(recoveryimage-deps) $(fujisan_recovery_kernel) $(fujisan_boot_signer) \
    $(fujisan_boot_signing_key) $(fujisan_boot_signing_cert)

	$(call pretty,"Target recovery image: $@")
	$(MKBOOTIMG) --kernel $(fujisan_recovery_kernel) --ramdisk $(recovery_ramdisk) \
	    --cmdline "$(INTERNAL_KERNEL_CMDLINE) androidboot.fujisan.recovery=1" --base $(BOARD_KERNEL_BASE) \
	    --pagesize $(BOARD_KERNEL_PAGESIZE) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) \
	    $(BOARD_RECOVERY_MKBOOTIMG_ARGS) --output $@.unsigned
	$(fujisan_boot_signer) /recovery $@.unsigned $(fujisan_boot_signing_key) \
	    $(fujisan_boot_signing_cert) $@
	$(hide) rm -f $@.unsigned
	$(call assert-max-image-size,$@,$(call get-hash-image-max-size,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE)))
