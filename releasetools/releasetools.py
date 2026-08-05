"""Fujisan OTA customizations."""


def _preserve_user_recovery(info):
    """Prevent the generated recovery patch from replacing TWRP after OTA."""
    info.script.Print("Preserving the installed recovery image")
    info.script.AppendExtra(
        'delete("/vendor/bin/install-recovery.sh", '
        '"/vendor/recovery-from-boot.p", '
        '"/vendor/etc/recovery-resource.dat", '
        '"/vendor/etc/recovery.img");\n')


def FullOTA_InstallEnd(info):
    _preserve_user_recovery(info)


def IncrementalOTA_InstallEnd(info):
    _preserve_user_recovery(info)
