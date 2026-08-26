#!/bin/bash
# Preserve Fujisan's legacy BootSignature trailer in target-files and OTA
# recovery images. AOSP reconstructs IMAGES/recovery.img from RECOVERY/, which
# bypasses BOARD_CUSTOM_BOOTIMG_MK and produces an unsigned image. The generic
# target-files recipe also saves the final, device-signed artifact in
# BOOTABLE_IMAGES/ when BOARD_CUSTOM_BOOTIMG is set; replace the reconstructed
# recovery copies with that authoritative signed image.
set -euo pipefail

host_out=""
for ((index = 1; index <= $#; index++)); do
    if [[ "${!index}" == "-p" ]]; then
        ((index++))
        host_out="${!index}"
        break
    fi
done

if [[ -z "${host_out}" ]]; then
    echo "fujisan add_img wrapper: missing -p <host-out>" >&2
    exit 1
fi

real_tool="${host_out}/bin/add_img_to_target_files"
target_files="${!#}"
if [[ ! -x "${real_tool}" ]]; then
    echo "fujisan add_img wrapper: missing ${real_tool}" >&2
    exit 1
fi

"${real_tool}" "$@"

signed_recovery="${target_files}/BOOTABLE_IMAGES/recovery.img"
if [[ ! -f "${signed_recovery}" ]]; then
    echo "fujisan add_img wrapper: signed recovery is missing: ${signed_recovery}" >&2
    exit 1
fi

install -m 0644 "${signed_recovery}" "${target_files}/IMAGES/recovery.img"
if [[ -e "${target_files}/OTA/recovery-two-step.img" ]]; then
    install -m 0644 "${signed_recovery}" "${target_files}/OTA/recovery-two-step.img"
fi
