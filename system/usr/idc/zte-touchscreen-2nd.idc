# Secondary panel touch (B). HWC reports panel B as EXTERNAL uniqueId local:1.
# device.internal must be 0 so InputReader matches the EXTERNAL viewport;
# otherwise isExternal=false and B touches fall back onto display 0 (panel A).
device.internal = 0
touch.deviceType = touchScreen
touch.orientationAware = 1
touch.displayId = local:1
