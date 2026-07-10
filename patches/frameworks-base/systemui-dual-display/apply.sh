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

def ensure_import(anchor, new_import):
    global updated
    if new_import in updated:
        return
    if anchor not in updated:
        print(f"Did not find import anchor {anchor.strip()} in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + new_import, 1)

ensure_import("import android.app.ActivityManager;\n", "import android.app.ActivityOptions;\n")
ensure_import("import android.content.BroadcastReceiver;\n", "import android.content.ComponentName;\n")

if "import android.view.Gravity;\n" not in updated:
    anchor = "import android.view.ViewGroup;\n"
    if anchor not in updated:
        print(f"Did not find Gravity import anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(anchor, anchor + "import android.view.Gravity;\n", 1)

constant_anchor = '''    private static final String EXTRA_DISABLE_STATE = "disabled_state";
'''
constant_extra_anchor = '''    private static final String EXTRA_FUJISAN_SECONDARY = "fujisan_secondary";
'''
constant_new = '''    private static final String EXTRA_FUJISAN_SECONDARY = "fujisan_secondary";
    private static final int FUJISAN_PRIMARY_DISPLAY_ID = 0;
    private static final int FUJISAN_SECONDARY_DISPLAY_ID = 1;
    private static final String FUJISAN_SECONDARY_HOME =
            "org.lineageos.trebuchet/com.android.launcher3.searchlauncher.SecondarySearchLauncher";
'''
if "FUJISAN_PRIMARY_DISPLAY_ID" not in updated:
    if constant_extra_anchor in updated:
        updated = updated.replace(constant_extra_anchor, constant_new, 1)
    elif constant_anchor in updated:
        updated = updated.replace(constant_anchor, constant_anchor + constant_new, 1)
    else:
        print(f"Did not find NavigationBarFragment constant anchor in {path}", file=sys.stderr)
        sys.exit(1)

field_anchor = '''    private LightBarController mLightBarController;
'''
field_new = '''    private LightBarController mLightBarController;
    private boolean mFujisanSecondary;
'''
if field_new not in updated:
    if field_anchor not in updated:
        print(f"Did not find NavigationBarFragment field anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(field_anchor, field_new, 1)

on_create_anchor = '''        super.onCreate(savedInstanceState);
        mCommandQueue = SysUiServiceProvider.getComponent(getContext(), CommandQueue.class);
'''
on_create_new = '''        super.onCreate(savedInstanceState);
        Bundle args = getArguments();
        mFujisanSecondary = args != null && args.getBoolean(EXTRA_FUJISAN_SECONDARY, false);
        mCommandQueue = SysUiServiceProvider.getComponent(getContext(), CommandQueue.class);
'''
if on_create_new not in updated:
    if on_create_anchor not in updated:
        print(f"Did not find NavigationBarFragment onCreate anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(on_create_anchor, on_create_new, 1)

helpers_anchor = '''    private void prepareNavigationBarView() {
        mNavigationBarView.reorient();
'''
helpers_new = '''    private int getFujisanTargetDisplayId() {
        return mFujisanSecondary ? FUJISAN_SECONDARY_DISPLAY_ID : FUJISAN_PRIMARY_DISPLAY_ID;
    }

    private boolean isFujisanHomeActivity(ComponentName component) {
        if (component == null) {
            return false;
        }
        final String className = component.getClassName();
        return "org.lineageos.trebuchet".equals(component.getPackageName())
                && (className.endsWith(".SearchLauncher")
                        || className.endsWith(".SecondarySearchLauncher"));
    }

    private int getTopTaskIdOnDisplay(int displayId, boolean includeHome) {
        try {
            int bestTaskId = -1;
            int bestPosition = Integer.MIN_VALUE;
            IActivityManager activityManager = ActivityManagerNative.getDefault();
            List<ActivityManager.StackInfo> stacks = activityManager.getAllStackInfos();
            for (int i = 0; stacks != null && i < stacks.size(); i++) {
                ActivityManager.StackInfo stack = stacks.get(i);
                if (stack == null || stack.displayId != displayId || stack.taskIds == null
                        || stack.taskIds.length == 0) {
                    continue;
                }
                if (!includeHome && isFujisanHomeActivity(stack.topActivity)) {
                    continue;
                }
                if (stack.position >= bestPosition) {
                    bestPosition = stack.position;
                    bestTaskId = stack.taskIds[stack.taskIds.length - 1];
                }
            }
            return bestTaskId;
        } catch (RemoteException e) {
            Log.w(TAG, "Unable to query display tasks", e);
            return -1;
        }
    }

    private boolean focusFujisanDisplayTask(int displayId, boolean includeHome) {
        final int taskId = getTopTaskIdOnDisplay(displayId, includeHome);
        if (taskId < 0) {
            return false;
        }
        try {
            ActivityManagerNative.getDefault().setFocusedTask(taskId);
            return true;
        } catch (RemoteException e) {
            Log.w(TAG, "Unable to focus display task " + taskId, e);
            return false;
        }
    }

    private boolean launchFujisanSecondaryHome() {
        try {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_HOME);
            intent.setComponent(ComponentName.unflattenFromString(FUJISAN_SECONDARY_HOME));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchDisplayId(FUJISAN_SECONDARY_DISPLAY_ID);
            getContext().startActivityAsUser(intent, options.toBundle(), UserHandle.CURRENT);
            return true;
        } catch (RuntimeException e) {
            Log.w(TAG, "Unable to launch secondary home", e);
            return false;
        }
    }

    private boolean cycleFujisanSecondaryTask() {
        try {
            IActivityManager activityManager = ActivityManagerNative.getDefault();
            List<ActivityManager.StackInfo> stacks = activityManager.getAllStackInfos();
            int topTaskId = -1;
            int previousTaskId = -1;
            int topPosition = Integer.MIN_VALUE;
            int previousPosition = Integer.MIN_VALUE;
            for (int i = 0; stacks != null && i < stacks.size(); i++) {
                ActivityManager.StackInfo stack = stacks.get(i);
                if (stack == null || stack.displayId != FUJISAN_SECONDARY_DISPLAY_ID
                        || stack.taskIds == null || stack.taskIds.length == 0
                        || isFujisanHomeActivity(stack.topActivity)) {
                    continue;
                }
                final int taskId = stack.taskIds[stack.taskIds.length - 1];
                if (stack.position >= topPosition) {
                    previousTaskId = topTaskId;
                    previousPosition = topPosition;
                    topTaskId = taskId;
                    topPosition = stack.position;
                } else if (stack.position > previousPosition) {
                    previousTaskId = taskId;
                    previousPosition = stack.position;
                }
            }
            final int taskId = previousTaskId >= 0 ? previousTaskId : topTaskId;
            if (taskId < 0) {
                return launchFujisanSecondaryHome();
            }
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchDisplayId(FUJISAN_SECONDARY_DISPLAY_ID);
            activityManager.moveTaskToFront(taskId, ActivityManager.MOVE_TASK_NO_USER_ACTION,
                    options.toBundle());
            activityManager.setFocusedTask(taskId);
            return true;
        } catch (RemoteException e) {
            Log.w(TAG, "Unable to switch secondary task", e);
            return false;
        }
    }

    private boolean onFujisanBackTouch(View v, MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            if (!focusFujisanDisplayTask(getFujisanTargetDisplayId(), !mFujisanSecondary)) {
                return mFujisanSecondary;
            }
        }
        return false;
    }

    private void prepareNavigationBarView() {
        mNavigationBarView.reorient();
'''
if helpers_new not in updated:
    if helpers_anchor not in updated:
        print(f"Did not find NavigationBarFragment helper anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(helpers_anchor, helpers_new, 1)

back_button_anchor = '''        ButtonDispatcher backButton = mNavigationBarView.getBackButton();
        backButton.setLongClickable(true);
        backButton.setOnLongClickListener(this::onLongPressBackRecents);
'''
back_button_new = '''        ButtonDispatcher backButton = mNavigationBarView.getBackButton();
        backButton.setOnTouchListener(this::onFujisanBackTouch);
        backButton.setLongClickable(true);
        backButton.setOnLongClickListener(this::onLongPressBackRecents);
'''
if back_button_new not in updated:
    if back_button_anchor not in updated:
        print(f"Did not find NavigationBarFragment back button anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(back_button_anchor, back_button_new, 1)

home_touch_anchor = '''    private boolean onHomeTouch(View v, MotionEvent event) {
        if (mHomeBlockedThisTouch && event.getActionMasked() != MotionEvent.ACTION_DOWN) {
            return true;
        }
'''
home_touch_new = '''    private boolean onHomeTouch(View v, MotionEvent event) {
        if (mFujisanSecondary) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    mStatusBar.awakenDreams();
                    return true;
                case MotionEvent.ACTION_UP:
                    mStatusBar.awakenDreams();
                    launchFujisanSecondaryHome();
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    return true;
            }
        } else if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            focusFujisanDisplayTask(FUJISAN_PRIMARY_DISPLAY_ID, true);
        }
        if (mHomeBlockedThisTouch && event.getActionMasked() != MotionEvent.ACTION_DOWN) {
            return true;
        }
'''
if home_touch_new not in updated:
    if home_touch_anchor not in updated:
        print(f"Did not find NavigationBarFragment home touch anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(home_touch_anchor, home_touch_new, 1)

recents_touch_anchor = '''    private boolean onRecentsTouch(View v, MotionEvent event) {
        int action = event.getAction() & MotionEvent.ACTION_MASK;
'''
recents_touch_new = '''    private boolean onRecentsTouch(View v, MotionEvent event) {
        if (mFujisanSecondary) {
            if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                mStatusBar.awakenDreams();
                cycleFujisanSecondaryTask();
            }
            return true;
        }
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            focusFujisanDisplayTask(FUJISAN_PRIMARY_DISPLAY_ID, true);
        }
        int action = event.getAction() & MotionEvent.ACTION_MASK;
'''
if recents_touch_new not in updated:
    if recents_touch_anchor not in updated:
        print(f"Did not find NavigationBarFragment recents touch anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(recents_touch_anchor, recents_touch_new, 1)

recents_click_anchor = '''    private void onRecentsClick(View v) {
        if (LatencyTracker.isEnabled(getContext())) {
'''
recents_click_new = '''    private void onRecentsClick(View v) {
        if (mFujisanSecondary) {
            mStatusBar.awakenDreams();
            cycleFujisanSecondaryTask();
            return;
        }
        focusFujisanDisplayTask(FUJISAN_PRIMARY_DISPLAY_ID, true);
        if (LatencyTracker.isEnabled(getContext())) {
'''
if recents_click_new not in updated:
    if recents_click_anchor not in updated:
        print(f"Did not find NavigationBarFragment recents click anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(recents_click_anchor, recents_click_new, 1)

current_vis_anchor = '''    public void setCurrentSysuiVisibility(int systemUiVisibility) {
        mSystemUiVisibility = systemUiVisibility;
'''
current_vis_new = '''    public void setCurrentSysuiVisibility(int systemUiVisibility) {
        if (mFujisanSecondary) {
            return;
        }
        mSystemUiVisibility = systemUiVisibility;
'''
if current_vis_new not in updated:
    if current_vis_anchor not in updated:
        print(f"Did not find NavigationBarFragment current visibility anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(current_vis_anchor, current_vis_new, 1)

sysui_vis_anchor = '''    public void setSystemUiVisibility(int vis, int fullscreenStackVis, int dockedStackVis,
            int mask, Rect fullscreenStackBounds, Rect dockedStackBounds) {
        final int oldVal = mSystemUiVisibility;
'''
sysui_vis_new = '''    public void setSystemUiVisibility(int vis, int fullscreenStackVis, int dockedStackVis,
            int mask, Rect fullscreenStackBounds, Rect dockedStackBounds) {
        if (mFujisanSecondary) {
            return;
        }
        final int oldVal = mSystemUiVisibility;
'''
if sysui_vis_new not in updated:
    if sysui_vis_anchor not in updated:
        print(f"Did not find NavigationBarFragment sysui visibility anchor in {path}", file=sys.stderr)
        sys.exit(1)
    updated = updated.replace(sysui_vis_anchor, sysui_vis_new, 1)

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
        if (!isRealNavigationBar) {
            Bundle args = new Bundle();
            args.putBoolean(EXTRA_FUJISAN_SECONDARY, true);
            fragment.setArguments(args);
        }
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
                || mFujisanSecondaryNavigationBarView != null) {
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

        if (shouldCreateFujisanSecondaryStatusBar()) {
            addFujisanSecondaryStatusBar(displayContext);
        }
    }

    private boolean shouldCreateFujisanSecondarySystemBars() {
        return SystemProperties.getBoolean("ro.feature.target_dual_display", false)
                && "4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"));
    }

    private boolean shouldCreateFujisanSecondaryStatusBar() {
        return SystemProperties.getBoolean("persist.vendor.fujisan.secondary_statusbar", false);
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
