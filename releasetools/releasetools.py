"""Fujisan OTA customizations."""


def _install_lineage_recovery(info):
    """Flash the signed full recovery image carried by every non-A/B OTA."""
    info.script.Print("Installing LineageOS recovery image")
    info.script.WriteRawImage("/recovery", "recovery.img")


def FullOTA_InstallEnd(info):
    _install_lineage_recovery(info)


def IncrementalOTA_InstallEnd(info):
    _install_lineage_recovery(info)
