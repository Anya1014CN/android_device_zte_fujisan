#!/system/bin/sh

set -eu

NV_FILE=/persist/.bt_nv.bin
OUT_DIR=/data/vendor/bluetooth
OUT_FILE=${OUT_DIR}/bluetooth_bdaddr

mkdir -p "${OUT_DIR}"

if [ -f "${NV_FILE}" ]; then
    addr="$(od -An -tx1 -j3 -N6 "${NV_FILE}" 2>/dev/null | tr -s '[:space:]' ' ' | sed 's/^ //' | tr ' ' ':' | tr '[:upper:]' '[:lower:]')"
else
    addr=""
fi

if [ -n "${addr}" ]; then
    printf '%s\n' "${addr}" > "${OUT_FILE}"
    chown bluetooth:bluetooth "${OUT_FILE}"
    chmod 0660 "${OUT_FILE}"
    setprop persist.service.bdroid.bdaddr "${addr}"
fi
