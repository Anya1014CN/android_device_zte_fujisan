Official GPS stack on stock CN Android 8.1 (`P996A26 / Axon M CN 2.10`):

- HAL service: `/vendor/bin/hw/vendor.qti.gnss@1.0-service`
- HAL registrations:
  - `android.hardware.gnss@1.0::IGnss/default`
  - `vendor.qti.gnss@1.0::ILocHidlGnss/gnss_vendor`
- Main daemons:
  - `loc_launcher`
  - `mlid`
  - `lowi-server`
  - `slim_daemon`
  - `xtra-daemon`
- Config files:
  - `/vendor/etc/gps.conf`
  - `/vendor/etc/izat.conf`
  - `/vendor/etc/xtwifi.conf`
  - `/vendor/etc/gpsInterval.xml`
- Init dependencies:
  - `/dev/socket/qmux_gps` created in `init.qcom.rc`
  - `/data/vendor/location`, `/data/vendor/location/mq`, `/data/vendor/location/xtwifi`
  - `qti_gnss_service` runs as `user gps`, `group system gps radio`
  - `loc_launcher` runs in `group gps inet diag wifi`

Bring-up note:
- Stock labels the QTI GNSS executables as `hal_gnss_qti_exec`, not `hal_gnss_default_exec`.
- If GPS bring-up fails later, verify this whole chain before changing framework-side location code.
