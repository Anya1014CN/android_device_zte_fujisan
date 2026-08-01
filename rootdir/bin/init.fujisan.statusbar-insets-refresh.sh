#!/system/bin/sh

# display_mode is published before HWC, WindowManager and SystemUI necessarily
# agree on the new folded area.  Poll their public dumpsys state after every
# posture transition.  Refresh only once WindowManager and the StatusBar window
# have reported and drawn the target geometry twice in succession.

is_target_size() {
    case "${1}:${2}" in
        zoom:2160x1920|zoom:1920x2160|single:1080x1920|single:1920x1080)
            return 0
            ;;
    esac
    return 1
}

refresh_insets() {
    mode="$(/system/bin/cmd uimode night | /system/bin/sed -n 's/^Night mode: //p')"
    case "${mode}" in
        yes)
            opposite=no
            ;;
        no|auto|custom)
            opposite=yes
            ;;
        *)
            /system/bin/log -p e -t FujisanStatusBarInsetsRefresh \
                "unknown night mode: ${mode}"
            return 1
            ;;
    esac

    # Keep these adjacent: invalidate the SystemUI cache without a visible
    # theme transition, then restore the selected user policy exactly.
    /system/bin/cmd uimode night "${opposite}"
    first_status=$?
    /system/bin/cmd uimode night "${mode}"
    restore_status=$?

    if [ "${first_status}" -eq 0 ] && [ "${restore_status}" -eq 0 ]; then
        /system/bin/log -t FujisanStatusBarInsetsRefresh \
            "cleared inset cache after ${target_mode} reached ${wm_cur}"
        return 0
    fi

    /system/bin/log -p e -t FujisanStatusBarInsetsRefresh \
        "UiMode refresh failed: switch=${first_status} restore=${restore_status}"
    return 1
}

stable_mode=""
stable_samples=0
attempt=0

while [ "${attempt}" -lt 24 ]; do
    attempt=$((attempt + 1))
    target_mode="$(/system/bin/getprop vendor.fujisan.display_mode)"
    case "${target_mode}" in
        single|zoom)
            ;;
        *)
            /system/bin/log -p w -t FujisanStatusBarInsetsRefresh \
                "unexpected display mode: ${target_mode}"
            exit 0
            ;;
    esac

    wm_dump="$(/system/bin/dumpsys window displays)"
    wm_cur="$(printf '%s\n' "${wm_dump}" | /system/bin/sed -n \
        's/.* cur=\([0-9][0-9]*x[0-9][0-9]*\).*/\1/p' | /system/bin/head -n 1)"
    wm_frames="$(printf '%s\n' "${wm_dump}" | /system/bin/sed -n \
        's/.*DisplayFrames w=\([0-9][0-9]*\) h=\([0-9][0-9]*\).*/\1x\2/p' | /system/bin/head -n 1)"

    statusbar_dump="$(/system/bin/dumpsys window windows | /system/bin/awk '
        /^[[:space:]]*Window #[0-9]+ .*StatusBar}:$/ { in_statusbar=1; next }
        in_statusbar && /^[[:space:]]*Window #[0-9]+ / { in_statusbar=0 }
        in_statusbar { print }
    ')"
    statusbar_bounds="$(printf '%s\n' "${statusbar_dump}" | /system/bin/sed -n \
        's/.*mLastReportedConfiguration=.*mBounds=Rect(0, 0 - \([0-9][0-9]*\), \([0-9][0-9]*\)).*/\1x\2/p' | /system/bin/head -n 1)"
    statusbar_width="$(printf '%s\n' "${statusbar_dump}" | /system/bin/sed -n \
        's/.*mFrame=\[0,0\]\[\([0-9][0-9]*\),[0-9][0-9]*\].*/\1/p' | /system/bin/head -n 1)"
    expected_width="${wm_cur%x*}"

    if is_target_size "${target_mode}" "${wm_cur}" && \
        [ "${wm_frames}" = "${wm_cur}" ] && \
        [ "${statusbar_bounds}" = "${wm_cur}" ] && \
        [ "${statusbar_width}" = "${expected_width}" ] && \
        printf '%s\n' "${wm_dump}" | /system/bin/grep -q 'mLayoutNeeded=false' && \
        printf '%s\n' "${wm_dump}" | /system/bin/grep -q 'mWindowManagerDrawComplete=true' && \
        printf '%s\n' "${statusbar_dump}" | /system/bin/grep -q 'mDrawState=HAS_DRAWN' && \
        printf '%s\n' "${statusbar_dump}" | /system/bin/grep -q 'isVisible=true'; then
        if [ "${stable_mode}" = "${target_mode}" ]; then
            stable_samples=$((stable_samples + 1))
        else
            stable_mode="${target_mode}"
            stable_samples=1
        fi

        if [ "${stable_samples}" -ge 2 ] && \
            [ "$(/system/bin/getprop vendor.fujisan.display_mode)" = "${target_mode}" ]; then
            refresh_insets
            exit $?
        fi
    else
        stable_mode=""
        stable_samples=0
    fi

    sleep 0.5
done

/system/bin/log -p w -t FujisanStatusBarInsetsRefresh \
    "timed out waiting for mode=${target_mode} wm=${wm_cur} frame=${wm_frames} statusbar=${statusbar_bounds}"
exit 1
