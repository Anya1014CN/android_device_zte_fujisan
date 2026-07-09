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
    local patch_ret=0

    if [ -n "$marker_path" ] && [ ! -f "$_fujisan_source_root/$marker_path" ]; then
        return 0
    fi

    if [ ! -f "$script_path" ]; then
        echo "fujisan: ERROR: missing patch script $script_path" >&2
        return 1
    fi

    echo "fujisan: applying patch $(basename "$(dirname "$script_path")")/$(basename "$script_path") ..."
    sh "$script_path" "$_fujisan_source_root" || patch_ret=$?

    if [ $patch_ret -ne 0 ]; then
        echo "fujisan: ERROR: patch $(basename "$(dirname "$script_path")") FAILED (exit code $patch_ret)" >&2
        echo "fujisan: The AOSP source tree may be out of sync. Run the following before re-lunching:" >&2
        echo "fujisan:   repo sync hardware/qcom/display-caf/msm8996" >&2
        echo "fujisan:   repo sync hardware/qcom/display/msm8996" >&2
        echo "fujisan:   repo sync frameworks/base" >&2
        return 1
    fi
}

echo "fujisan: applying dual-screen patches ..."

_fujisan_apply_patch \
    "$_fujisan_device_dir/patches/display-caf/msm8996/apply.sh" \
    "" || { unset -f _fujisan_apply_patch; return 1; }

_fujisan_apply_patch \
    "$_fujisan_device_dir/patches/frameworks-base/local-display-adapter/apply.sh" \
    "frameworks/base/services/core/java/com/android/server/display/LocalDisplayAdapter.java" || { unset -f _fujisan_apply_patch; return 1; }

_fujisan_apply_patch \
    "$_fujisan_device_dir/patches/frameworks-base/systemui-keyguard/apply.sh" \
    "frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java" || { unset -f _fujisan_apply_patch; return 1; }

_fujisan_apply_patch \
    "$_fujisan_device_dir/patches/frameworks-native/surfaceflinger-dual/apply.sh" \
    "frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp" || { unset -f _fujisan_apply_patch; return 1; }

# Invalidate build cache for patched files to ensure recompilation.
for _f in \
    hardware/qcom/display/msm8996/libqdutils/display_config.h \
    hardware/qcom/display/msm8996/sdm/libs/hwc2/hwc_session.cpp \
    frameworks/base/services/core/java/com/android/server/display/LocalDisplayAdapter.java \
    frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java \
    frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
do
    if [ -f "$_fujisan_source_root/$_f" ]; then
        touch "$_fujisan_source_root/$_f"
    fi
done

echo "fujisan: all dual-screen patches applied successfully."

unset -f _fujisan_apply_patch
unset _fujisan_device_dir
unset _fujisan_source_root
