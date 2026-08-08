# Fujisan's legacy bootloader verifies the AOSP BootSignature DER trailer on
# boot and recovery images. The matching public AOSP verity test certificate
# is the certificate used by the previously accepted device image.

fujisan_boot_signer := $(HOST_OUT_EXECUTABLES)/boot_signer
fujisan_boot_signing_key := $(DEVICE_PATH)/keys/verity.pk8
fujisan_boot_signing_cert := $(DEVICE_PATH)/keys/verity.x509.pem

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

$(INSTALLED_RECOVERYIMAGE_TARGET): $(recoveryimage-deps) $(fujisan_boot_signer) \
    $(fujisan_boot_signing_key) $(fujisan_boot_signing_cert)

	$(call pretty,"Target recovery image: $@")
	$(call build-recoveryimage-target,$@)
	$(fujisan_boot_signer) /recovery $@ $(fujisan_boot_signing_key) \
	    $(fujisan_boot_signing_cert) $@
	$(call assert-max-image-size,$@,$(call get-hash-image-max-size,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE)))
