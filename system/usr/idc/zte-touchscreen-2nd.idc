# B's target display is selected dynamically by fujisan_halld: local:0 in
# A+B zoom mode, local:1 in independent-display mode.  Do not set
# touch.displayId here: a static IDC value overrides InputManager's runtime
# association and disables B whenever that display is not present.
device.internal = 1
touch.deviceType = touchScreen
touch.orientationAware = 1
