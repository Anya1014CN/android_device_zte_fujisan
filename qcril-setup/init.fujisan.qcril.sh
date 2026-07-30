#!/vendor/bin/sh
# Fujisan QCRIL database IMS configuration patch
# Runs once during boot to ensure IMS/VoLTE config is properly initialized.

QCRIL_DB=/vendor/radio/qcril_database/qcril.db
if [ ! -f "$QCRIL_DB" ]; then
    exit 0
fi

log -t qcril_setup "Patching QCRIL database for IMS/VoLTE..."

# Force IMS data call and VoLTE registration
sqlite3 "$QCRIL_DB" << 'SQL'
INSERT OR IGNORE INTO qcril_properties_table (property_name, property_value)
VALUES ('persist.vendor.radio.force_on_dc_ims','1');
INSERT OR IGNORE INTO qcril_properties_table (property_name, property_value)
VALUES ('persist.vendor.radio.ims_pdn_req','1');
INSERT OR IGNORE INTO qcril_properties_table (property_name, property_value)
VALUES ('persist.vendor.radio.jbims','1');
SQL

log -t qcril_setup "QCRIL IMS patch applied."
