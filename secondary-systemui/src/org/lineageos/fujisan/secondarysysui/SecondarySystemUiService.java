package org.lineageos.fujisan.secondarysysui;

import android.app.ActivityOptions;
import android.app.Instrumentation;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.os.BatteryManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemProperties;
import android.provider.Settings;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public final class SecondarySystemUiService extends Service {
    private static final int SECONDARY_DISPLAY_ID = 1;
    private static final int BAR_BG = 0xcc000000;
    private static final int TEXT = Color.WHITE;
    private static final long REFRESH_MS = 2000;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final SimpleDateFormat mClockFormat =
            new SimpleDateFormat("HH:mm", Locale.US);

    private DisplayManager mDisplayManager;
    private Context mDisplayContext;
    private WindowManager mWindowManager;
    private View mStatusBar;
    private View mNavigationBar;
    private TextView mClockView;
    private TextView mBatteryView;
    private int mBatteryLevel = -1;
    private boolean mLaunchedHome;

    private final Runnable mRefreshRunnable = new Runnable() {
        @Override
        public void run() {
            refresh();
            mHandler.postDelayed(this, REFRESH_MS);
        }
    };

    private final BroadcastReceiver mBatteryReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_BATTERY_CHANGED.equals(intent.getAction())) {
                int level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
                if (level >= 0 && scale > 0) {
                    mBatteryLevel = level * 100 / scale;
                    updateText();
                }
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        mDisplayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        registerReceiver(mBatteryReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        mHandler.post(mRefreshRunnable);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        refresh();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        mHandler.removeCallbacksAndMessages(null);
        unregisterReceiver(mBatteryReceiver);
        removeBars();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void refresh() {
        if (shouldShow()) {
            ensureBars();
            updateText();
        } else {
            removeBars();
            maybeLaunchHomeOnSecondary();
        }
    }

    private boolean shouldShow() {
        return false;
    }

    private void maybeLaunchHomeOnSecondary() {
        if (mLaunchedHome || mDisplayManager == null
                || mDisplayManager.getDisplay(SECONDARY_DISPLAY_ID) == null
                || !"4".equals(SystemProperties.get("persist.vendor.fujisan.display_mode", "1"))) {
            return;
        }
        mLaunchedHome = true;
        launchHomeOnSecondary();
    }

    private void ensureBars() {
        if (mWindowManager == null) {
            Display display = mDisplayManager.getDisplay(SECONDARY_DISPLAY_ID);
            if (display == null) {
                return;
            }
            mDisplayContext = createDisplayContext(display);
            mWindowManager = (WindowManager) mDisplayContext.getSystemService(WINDOW_SERVICE);
        }

        if (mStatusBar == null) {
            mStatusBar = buildStatusBar();
            if (!addViewSafely(mStatusBar, makeBarLayoutParams(
                    Gravity.TOP, dp(24), "FujisanSecondaryStatusBar"))) {
                mStatusBar = null;
            }
        }

        if (mNavigationBar == null) {
            mNavigationBar = buildNavigationBar();
            if (!addViewSafely(mNavigationBar, makeBarLayoutParams(
                    Gravity.BOTTOM, dp(56), "FujisanSecondaryNavigationBar"))) {
                mNavigationBar = null;
            }
        }
    }

    private boolean addViewSafely(View view, WindowManager.LayoutParams lp) {
        try {
            mWindowManager.addView(view, lp);
            return true;
        } catch (RuntimeException ignored) {
            return false;
        }
    }

    private WindowManager.LayoutParams makeBarLayoutParams(int gravity, int height, String title) {
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                height,
                WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT);
        lp.gravity = gravity;
        lp.setTitle(title);
        lp.privateFlags |= WindowManager.LayoutParams.PRIVATE_FLAG_SHOW_FOR_ALL_USERS;
        return lp;
    }

    private View buildStatusBar() {
        LinearLayout bar = new LinearLayout(mDisplayContext);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(12), 0, dp(12), 0);
        bar.setBackgroundColor(BAR_BG);

        TextView label = makeText("B", 13, Gravity.CENTER_VERTICAL);
        mClockView = makeText("", 13, Gravity.CENTER);
        mBatteryView = makeText("", 13, Gravity.CENTER_VERTICAL | Gravity.RIGHT);

        bar.addView(label, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.MATCH_PARENT, 1f));
        bar.addView(mClockView, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.MATCH_PARENT, 1f));
        bar.addView(mBatteryView, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.MATCH_PARENT, 1f));
        return bar;
    }

    private View buildNavigationBar() {
        LinearLayout bar = new LinearLayout(mDisplayContext);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER);
        bar.setBackgroundColor(BAR_BG);
        bar.setPadding(dp(16), 0, dp(16), 0);

        bar.addView(makeNavButton("<", new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                sendKey(KeyEvent.KEYCODE_BACK);
            }
        }));
        bar.addView(makeNavButton("O", new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                launchHomeOnSecondary();
            }
        }));
        bar.addView(makeNavButton("[]", new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                sendKey(KeyEvent.KEYCODE_APP_SWITCH);
            }
        }));
        return bar;
    }

    private TextView makeNavButton(String text, View.OnClickListener listener) {
        TextView view = makeText(text, 24, Gravity.CENTER);
        view.setOnClickListener(listener);
        view.setAllCaps(false);
        view.setBackgroundColor(Color.TRANSPARENT);
        view.setPadding(0, 0, 0, dp(2));
        view.setLayoutParams(new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.MATCH_PARENT, 1f));
        return view;
    }

    private TextView makeText(String text, int sp, int gravity) {
        TextView view = new TextView(mDisplayContext);
        view.setText(text);
        view.setTextColor(TEXT);
        view.setTextSize(sp);
        view.setGravity(gravity);
        view.setIncludeFontPadding(false);
        return view;
    }

    private void updateText() {
        if (mClockView != null) {
            mClockView.setText(mClockFormat.format(new Date()));
        }
        if (mBatteryView != null) {
            mBatteryView.setText(mBatteryLevel >= 0 ? mBatteryLevel + "%" : "");
        }
    }

    private void launchHomeOnSecondary() {
        Intent intent = new Intent(this, SecondaryLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(SECONDARY_DISPLAY_ID);
        try {
            startActivity(intent, options.toBundle());
        } catch (RuntimeException e) {
            Intent fallback = new Intent(Intent.ACTION_MAIN);
            fallback.addCategory(Intent.CATEGORY_HOME);
            fallback.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                startActivity(fallback, options.toBundle());
            } catch (RuntimeException ignored) {
            }
        }
    }

    private void sendKey(final int keyCode) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    new Instrumentation().sendKeyDownUpSync(keyCode);
                } catch (RuntimeException ignored) {
                }
            }
        }, "fujisan-secondary-key").start();
    }

    private void removeBars() {
        if (mWindowManager != null) {
            if (mStatusBar != null) {
                mWindowManager.removeViewImmediate(mStatusBar);
                mStatusBar = null;
            }
            if (mNavigationBar != null) {
                mWindowManager.removeViewImmediate(mNavigationBar);
                mNavigationBar = null;
            }
        }
        mWindowManager = null;
        mDisplayContext = null;
        mClockView = null;
        mBatteryView = null;
    }

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
    }
}
