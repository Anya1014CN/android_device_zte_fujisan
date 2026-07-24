# Fujisan dual-LCD / hinge (LOS 19.1)

## Goal
- **Closed**: single primary (A face or B face)
- **Open**: virtual **2160×1920** large screen (left A + right B, 1px hinge at x=1080)
- No AOSP framework patches — device tree modules + kernel only

## Hall (mxm1120 / ah1898 compat)
| status | posture | mode | panels |
|--------|---------|------|--------|
| 1 | closed_a | single | A on, B off |
| 2 | open | zoom | A+B on, HWC split/mirror |
| 3 | closed_b | single | A composing (BL 0), B on (HWC copy) |

Sysfs: `/sys/module/ah1898/parameters/hall_status` (mirrors mxm1120).
Props: `vendor.fujisan.hall_status`, `device_state`, `display_mode`, `active_primary`, `display_power`.

## Components
- `fujisan_halld` — posture props + panel power (respects `display_power` for sleep/wake)
- `hwcomposer.fujisan` — wraps msm8996; single passthrough; closed_b copies to fb1; zoom splits 2160 or mirrors 1080 interim
- DeviceState XML — SW_LID closed/open for 12L

## Primary switch (stock QS equivalent v1)
- `persist.vendor.fujisan.primary_panel=a|b`
- `persist.vendor.fujisan.primary_force=1` to pin while closed

## Sleep
HWC `SetPowerMode` sets `vendor.fujisan.display_power` and blanks B on sleep; halld will not re-light B until wake.
