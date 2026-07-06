#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET="$ROOT/frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java"

if [ ! -f "$TARGET" ]; then
  echo "Missing target file: $TARGET" >&2
  exit 1
fi

python3 - "$TARGET" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

listener_old = '''                    mStatusBarView.setPanel(mNotificationPanel);
                    mStatusBarView.setScrimController(mScrimController);
                    mStatusBarView.setBouncerShowing(mBouncerShowing);
                    setAreThereNotifications();
                    checkBarModes();'''

listener_new = '''                    mStatusBarView.setPanel(mNotificationPanel);
                    mStatusBarView.setScrimController(mScrimController);
                    mStatusBarView.setBouncerShowing(mBouncerShowing);
                    setAreThereNotifications();
                    checkBarModes();
                    updatePanelExpansionForKeyguard();'''

method_old = '''    private void updatePanelExpansionForKeyguard() {
        if (mState == StatusBarState.KEYGUARD && mFingerprintUnlockController.getMode()
                != FingerprintUnlockController.MODE_WAKE_AND_UNLOCK) {
            instantExpandNotificationsPanel();
        } else if (mState == StatusBarState.FULLSCREEN_USER_SWITCHER) {
            instantCollapseNotificationPanel();
        }
    }'''

method_new = '''    private void updatePanelExpansionForKeyguard() {
        if (mStatusBarView == null) {
            return;
        }
        if (mState == StatusBarState.KEYGUARD && mFingerprintUnlockController.getMode()
                != FingerprintUnlockController.MODE_WAKE_AND_UNLOCK) {
            instantExpandNotificationsPanel();
        } else if (mState == StatusBarState.FULLSCREEN_USER_SWITCHER) {
            instantCollapseNotificationPanel();
        }
    }'''

updated = text

if listener_new not in updated:
    if listener_old not in updated:
        print(f"Did not find expected status bar fragment block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(listener_old, listener_new, 1)

if method_new not in updated:
    if method_old not in updated:
        print(f"Did not find expected keyguard panel expansion method in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(method_old, method_new, 1)

if updated == text:
    print(f"SystemUI keyguard race compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Deferred keyguard panel expansion until status bar view is ready in {path}")
PY
