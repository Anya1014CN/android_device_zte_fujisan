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
updated = text

def ensure_import(anchor, new_import):
    global updated
    if new_import in updated:
        return
    if anchor not in updated:
        print(f"Did not find import anchor {anchor.strip()} in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + new_import, 1)

ensure_import("import android.graphics.Bitmap;\n", "import android.hardware.display.DisplayManager;\n")
ensure_import("import android.os.AsyncTask;\n", "import android.os.Binder;\n")
ensure_import("import android.view.Display;\n", "import android.view.Gravity;\n")

field_old = '''    private NavigationBarFragment mNavigationBar;
    private View mNavigationBarView;
'''
field_new = '''    private static final int FUJISAN_SECONDARY_DISPLAY_ID = 1;

    private NavigationBarFragment mNavigationBar;
    private View mNavigationBarView;
    private NavigationBarFragment mFujisanSecondaryNavigationBar;
    private View mFujisanSecondaryNavigationBarView;
    private View mFujisanSecondaryStatusBarView;
    private WindowManager mFujisanSecondaryWindowManager;
'''
if field_new not in updated:
    if field_old not in updated:
        print(f"Did not find navigation bar field block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(field_old, field_new, 1)

create_old = '''    protected void createNavigationBar() {
        mNavigationBarView = NavigationBarFragment.create(mContext, (tag, fragment) -> {
            mNavigationBar = (NavigationBarFragment) fragment;
            if (mLightBarController != null) {
                mNavigationBar.setLightBarController(mLightBarController);
            }
            mNavigationBar.setCurrentSysuiVisibility(mSystemUiVisibility);
        });
    }
'''
create_new = '''    protected void createNavigationBar() {
        mNavigationBarView = NavigationBarFragment.create(mContext, (tag, fragment) -> {
            mNavigationBar = (NavigationBarFragment) fragment;
            if (mLightBarController != null) {
                mNavigationBar.setLightBarController(mLightBarController);
            }
            mNavigationBar.setCurrentSysuiVisibility(mSystemUiVisibility);
        });
        createFujisanSecondarySystemBars();
    }

    private void createFujisanSecondarySystemBars() {
        if (!shouldCreateFujisanSecondarySystemBars()
                || mFujisanSecondaryNavigationBarView != null
                || mFujisanSecondaryStatusBarView != null) {
            return;
        }

        final DisplayManager displayManager =
                (DisplayManager) mContext.getSystemService(Context.DISPLAY_SERVICE);
        if (displayManager == null) {
            return;
        }
        final Display display = displayManager.getDisplay(FUJISAN_SECONDARY_DISPLAY_ID);
        if (display == null) {
            return;
        }

        final Context displayContext = mContext.createDisplayContext(display);
        mFujisanSecondaryWindowManager =
                (WindowManager) displayContext.getSystemService(Context.WINDOW_SERVICE);
        if (mFujisanSecondaryWindowManager == null) {
            return;
        }

        mFujisanSecondaryNavigationBarView = NavigationBarFragment.create(displayContext,
                (tag, fragment) -> {
                    mFujisanSecondaryNavigationBar = (NavigationBarFragment) fragment;
                    if (mLightBarController != null) {
                        mFujisanSecondaryNavigationBar.setLightBarController(mLightBarController);
                    }
                    mFujisanSecondaryNavigationBar.setCurrentSysuiVisibility(mSystemUiVisibility);
                });

        addFujisanSecondaryStatusBar(displayContext);
    }

    private boolean shouldCreateFujisanSecondarySystemBars() {
        return SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"));
    }

    private void addFujisanSecondaryStatusBar(Context displayContext) {
        final PhoneStatusBarView statusBarView = (PhoneStatusBarView) LayoutInflater
                .from(displayContext).inflate(R.layout.status_bar, null);
        statusBarView.setBar(this);
        statusBarView.setPanel(mNotificationPanel);
        statusBarView.setScrimController(mScrimController);
        statusBarView.setBouncerShowing(mBouncerShowing);
        statusBarView.setOnTouchListener((v, event) -> true);

        final WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                getStatusBarHeight(),
                WindowManager.LayoutParams.TYPE_STATUS_BAR,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS,
                PixelFormat.TRANSLUCENT);
        lp.token = new Binder();
        lp.gravity = Gravity.TOP;
        lp.setTitle("FujisanSecondaryStatusBar");
        lp.packageName = displayContext.getPackageName();

        try {
            mFujisanSecondaryWindowManager.addView(statusBarView, lp);
            mFujisanSecondaryStatusBarView = statusBarView;
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to add secondary status bar", e);
            mFujisanSecondaryStatusBarView = null;
        }
    }
'''
if create_new not in updated:
    if create_old not in updated:
        print(f"Did not find createNavigationBar block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(create_old, create_new, 1)

visibility_old = '''        mLightBarController.onSystemUiVisibilityChanged(fullscreenStackVis, dockedStackVis,
                mask, fullscreenStackBounds, dockedStackBounds, sbModeChanged, mStatusBarMode);
    }

    void touchAutoHide() {
'''
visibility_new = '''        mLightBarController.onSystemUiVisibilityChanged(fullscreenStackVis, dockedStackVis,
                mask, fullscreenStackBounds, dockedStackBounds, sbModeChanged, mStatusBarMode);
        if (mFujisanSecondaryNavigationBar != null) {
            mFujisanSecondaryNavigationBar.setCurrentSysuiVisibility(mSystemUiVisibility);
        }
    }

    void touchAutoHide() {
'''
if visibility_new not in updated:
    if visibility_old not in updated:
        print(f"Did not find setSystemUiVisibility tail block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(visibility_old, visibility_new, 1)

destroy_old = '''        if (mNavigationBarView != null) {
            mWindowManager.removeViewImmediate(mNavigationBarView);
            mNavigationBarView = null;
        }
'''
destroy_new = '''        if (mNavigationBarView != null) {
            mWindowManager.removeViewImmediate(mNavigationBarView);
            mNavigationBarView = null;
        }
        removeFujisanSecondarySystemBars();
'''
if destroy_new not in updated:
    if destroy_old not in updated:
        print(f"Did not find destroy navigation bar block in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(destroy_old, destroy_new, 1)

remove_anchor = '''    public void destroy() {
'''
remove_helper = '''    private void removeFujisanSecondarySystemBars() {
        if (mFujisanSecondaryWindowManager != null) {
            if (mFujisanSecondaryStatusBarView != null) {
                mFujisanSecondaryWindowManager.removeViewImmediate(mFujisanSecondaryStatusBarView);
                mFujisanSecondaryStatusBarView = null;
            }
            if (mFujisanSecondaryNavigationBarView != null) {
                mFujisanSecondaryWindowManager.removeViewImmediate(
                        mFujisanSecondaryNavigationBarView);
                mFujisanSecondaryNavigationBarView = null;
            }
        }
        mFujisanSecondaryNavigationBar = null;
        mFujisanSecondaryWindowManager = null;
    }

'''
if remove_helper not in updated:
    if remove_anchor not in updated:
        print(f"Did not find remove helper anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(remove_anchor, remove_helper + remove_anchor, 1)

if updated == text:
    print(f"Fujisan dual-display SystemUI compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Added Fujisan dual-display SystemUI bars in {path}")
PY
