add_lunch_combo lineage_fujisan-eng
add_lunch_combo lineage_fujisan-userdebug
add_lunch_combo lineage_fujisan-user

if [ -n "${TOP-}" ]; then
    _fujisan_source_root="$TOP"
elif command -v gettop >/dev/null 2>&1 && [ -n "$(gettop)" ]; then
    _fujisan_source_root="$(gettop)"
else
    _fujisan_source_root="$(pwd)"
fi

if [ -n "${FUJISAN_DEVICE_DIR-}" ]; then
    _fujisan_device_dir="$FUJISAN_DEVICE_DIR"
else
    _fujisan_device_dir="$_fujisan_source_root/device/zte/fujisan"
fi

_fujisan_apply_patch() {
    local script_path="$1"
    local marker_path="$2"

    if [ ! -f "$_fujisan_source_root/$marker_path" ]; then
        return 0
    fi

    if [ ! -f "$script_path" ]; then
        echo "fujisan: missing patch script $script_path" >&2
        return 1
    fi

    sh "$script_path" "$_fujisan_source_root"
}

if [ "${FUJISAN_PATCH_ROOT-}" != "$_fujisan_source_root" ]; then
    export FUJISAN_PATCH_ROOT="$_fujisan_source_root"
    _fujisan_apply_patch \
        "$_fujisan_device_dir/patches/display-caf/msm8996/apply.sh" \
        "hardware/qcom/display-caf/msm8996/libqdutils/qdMetaData.cpp"
    _fujisan_apply_patch \
        "$_fujisan_device_dir/patches/frameworks-base/systemui-keyguard/apply.sh" \
        "frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java"
fi

unset -f _fujisan_apply_patch
unset _fujisan_device_dir
unset _fujisan_source_root
