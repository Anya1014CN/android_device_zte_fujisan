#!/bin/sh
set -eu

ROOT="${1:-$PWD}"
TARGET_STATUS="$ROOT/frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java"
TARGET_NAV="$ROOT/frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/NavigationBarFragment.java"

if [ ! -f "$TARGET_STATUS" ]; then
  echo "Missing target file: $TARGET_STATUS" >&2
  exit 1
fi

if [ ! -f "$TARGET_NAV" ]; then
  echo "Missing target file: $TARGET_NAV" >&2
  exit 1
fi

python3 - "$TARGET_NAV" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
updated = text

if "import android.view.Gravity;\n" not in updated:
    anchor = "import android.view.ViewGroup;\n"
    if anchor not in updated:
        print(f"Did not find Gravity import anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + "import android.view.Gravity;\n", 1)

old = '''    public static View create(Context context, FragmentListener listener) {
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_NAVIGATION_BAR,
                WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_SLIPPERY,
                PixelFormat.TRANSLUCENT);
        lp.token = new Binder();
        lp.setTitle("NavigationBar");
        lp.windowAnimations = 0;

        View navigationBarView = LayoutInflater.from(context).inflate(
                R.layout.navigation_bar_window, null);

        if (DEBUG) Log.v(TAG, "addNavigationBar: about to add " + navigationBarView);
        if (navigationBarView == null) return null;

        context.getSystemService(WindowManager.class).addView(navigationBarView, lp);
        FragmentHostManager fragmentHost = FragmentHostManager.get(navigationBarView);
        NavigationBarFragment fragment = new NavigationBarFragment();
        fragmentHost.getFragmentManager().beginTransaction()
                .replace(R.id.navigation_bar_frame, fragment, TAG)
                .commit();
        fragmentHost.addTagListener(TAG, listener);
        return navigationBarView;
    }
'''

new = '''    public static View create(Context context, FragmentListener listener) {
        return create(context, listener, WindowManager.LayoutParams.TYPE_NAVIGATION_BAR,
                "NavigationBar");
    }

    public static View create(Context context, FragmentListener listener, int windowType,
            String title) {
        final boolean isRealNavigationBar =
                windowType == WindowManager.LayoutParams.TYPE_NAVIGATION_BAR;
        final int windowHeight = isRealNavigationBar
                ? LayoutParams.MATCH_PARENT
                : context.getResources().getDimensionPixelSize(
                        com.android.internal.R.dimen.navigation_bar_height);
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                LayoutParams.MATCH_PARENT, windowHeight,
                windowType,
                WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_SLIPPERY,
                PixelFormat.TRANSLUCENT);
        lp.token = new Binder();
        lp.setTitle(title);
        lp.windowAnimations = 0;
        if (!isRealNavigationBar) {
            lp.gravity = Gravity.BOTTOM;
        }

        View navigationBarView = LayoutInflater.from(context).inflate(
                R.layout.navigation_bar_window, null);

        if (DEBUG) Log.v(TAG, "addNavigationBar: about to add " + navigationBarView);
        if (navigationBarView == null) return null;

        context.getSystemService(WindowManager.class).addView(navigationBarView, lp);
        FragmentHostManager fragmentHost = FragmentHostManager.get(navigationBarView);
        NavigationBarFragment fragment = new NavigationBarFragment();
        fragmentHost.getFragmentManager().beginTransaction()
                .replace(R.id.navigation_bar_frame, fragment, TAG)
                .commit();
        fragmentHost.addTagListener(TAG, listener);
        return navigationBarView;
    }
'''

previous_new = '''    public static View create(Context context, FragmentListener listener) {
        return create(context, listener, WindowManager.LayoutParams.TYPE_NAVIGATION_BAR,
                "NavigationBar");
    }

    public static View create(Context context, FragmentListener listener, int windowType,
            String title) {
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                windowType,
                WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_SLIPPERY,
                PixelFormat.TRANSLUCENT);
        lp.token = new Binder();
        lp.setTitle(title);
        lp.windowAnimations = 0;

        View navigationBarView = LayoutInflater.from(context).inflate(
                R.layout.navigation_bar_window, null);

        if (DEBUG) Log.v(TAG, "addNavigationBar: about to add " + navigationBarView);
        if (navigationBarView == null) return null;

        context.getSystemService(WindowManager.class).addView(navigationBarView, lp);
        FragmentHostManager fragmentHost = FragmentHostManager.get(navigationBarView);
        NavigationBarFragment fragment = new NavigationBarFragment();
        fragmentHost.getFragmentManager().beginTransaction()
                .replace(R.id.navigation_bar_frame, fragment, TAG)
                .commit();
        fragmentHost.addTagListener(TAG, listener);
        return navigationBarView;
    }
'''

if new not in updated:
    if previous_new in updated:
        updated = updated.replace(previous_new, new, 1)
    elif old in updated:
        updated = updated.replace(old, new, 1)
    else:
        print(f"Did not find NavigationBarFragment create block in {path}", file=sys.stderr)
        sys.exit(1)

if updated == text:
    print(f"NavigationBarFragment secondary window compatibility already updated in {path}")
    sys.exit(0)

path.write_text(updated)
print(f"Added NavigationBarFragment secondary window compatibility in {path}")
PY

python3 - "$TARGET_STATUS" <<'PY'
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

        try {
            mFujisanSecondaryNavigationBarView = NavigationBarFragment.create(displayContext,
                    (tag, fragment) -> {
                        mFujisanSecondaryNavigationBar = (NavigationBarFragment) fragment;
                        if (mLightBarController != null) {
                            mFujisanSecondaryNavigationBar.setLightBarController(
                                    mLightBarController);
                        }
                        mFujisanSecondaryNavigationBar.setCurrentSysuiVisibility(
                                mSystemUiVisibility);
                    }, WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
                    "FujisanSecondaryNavigationBar");
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to add secondary navigation bar", e);
            mFujisanSecondaryNavigationBarView = null;
        }

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
                WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
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
