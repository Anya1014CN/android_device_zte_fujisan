# Fujisan dual-LCD / hinge (LOS 19.1)

## Stock (9008_CNA8) findings
- Framework service `com.zte.duallcd.DisplayModeManagerService` (not AOSP).
- Modes: SINGLE / MIRROR / DOCKED / ZOOM (2160x1920 virtual large).
- Hall: `/sys/module/ah1898/parameters/hall_status` with A=1, B=2(open), C=3.
- Kernel sensor is mxm1120 (REL_X status + hall GPIO); we export stock-compat `ah1898`.

## This port (no AOSP framework patches)
1. **Closed**: single primary panel (A or B). `fujisan_halld` blanks the unused panel.
2. **Open (B)**: `vendor.fujisan.display_mode=zoom`; HWC advertises 2160x1920 config + hinge path.
3. **DeviceState** `vendor/etc/devicestate/device_state_configuration.xml` maps SW_LID closed/open.
4. **Primary switch**: `persist.vendor.fujisan.primary_panel=a|b` and `persist.vendor.fujisan.primary_force=1` to pin choice (stock QS tile equivalent for v1).

## Hinge
- 1px seam at x=1080 in the 2160 open layout (left A 0..1079, hinge 1080, right B 1081..2159).

## Not in v1
- Full stock DisplayModeManager (mirror/docked/zoom app policies).
- Dual TYPE_INTERNAL secondary hotplug (disabled; caused pink/snow).
