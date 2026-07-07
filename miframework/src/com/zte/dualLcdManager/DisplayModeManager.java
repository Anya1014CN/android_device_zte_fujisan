package com.zte.dualLcdManager;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import java.lang.reflect.Method;
import java.util.List;

public class DisplayModeManager {
    public static final int DISPLAY_MODE_SINGLE = 1;
    public static final int DISPLAY_MODE_ZOOM = 2;
    public static final int DISPLAY_MODE_DOCKED = 4;
    public static final int DISPLAY_MODE_MIRROR = 8;

    private static final String TAG = "DisplayModeManager";
    private static final String ACTION_MODE_SWITCHED = "com.zte.action.displaymode.switched";
    private static final String ACTION_TASK_SWITCH = "com.zte.action.displaymode.taskswitch";
    private static final String PROP_DEFAULT_MODE = "persist.vendor.fujisan.display_mode";
    private static final String PROP_DEFAULT_SINGLE_DISPLAY = "persist.vendor.fujisan.single_display_id";
    private static final String PROP_HALL_STATUS = "persist.sys.zte.hallStatus";
    private static final String PROP_DOCK_ORIENTATION = "persist.sys.zte.dockori";
    private static final String SETTING_DISPLAY_MODE = "display_mode";
    private static final String SETTING_SINGLE_DISPLAY = "displayid_single_mode";
    private static final String SETTING_USER_SINGLE_DISPLAY = "user_choose_displayid_single_mode";
    private static final String SETTING_HALL_OPEN_MODE = "hallC_display_mode";
    private static final String SETTING_DOCK_ORIENTATION = "dual_screen_dock_mode_screen_orientation_mode";
    private static final int HALL_STATUS_OPEN = 3;
    private static final int SINGLE_DISPLAY_A = 0;
    private static final int SINGLE_DISPLAY_B = 1;

    private static DisplayModeManager sInstance;

    private final Context mContext;

    private DisplayModeManager(Context context) {
        mContext = context != null ? context.getApplicationContext() : null;
    }

    public static synchronized DisplayModeManager getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new DisplayModeManager(context);
        }
        return sInstance;
    }

    public int getCurrentMode() {
        return getSystemInt(SETTING_DISPLAY_MODE, getPropInt(PROP_DEFAULT_MODE, DISPLAY_MODE_SINGLE));
    }

    public int getFocusDisplayId() {
        if (getCurrentMode() == DISPLAY_MODE_SINGLE && getCurrentSingleDisplay() == SINGLE_DISPLAY_B) {
            return 1;
        }
        return 0;
    }

    public int getHallSensorStatus() {
        return getPropInt(PROP_HALL_STATUS, 1);
    }

    public boolean switchMode(int mode) {
        if (!isValidMode(mode)) {
            return false;
        }

        putSystemInt(SETTING_DISPLAY_MODE, mode);
        putSystemInt(SETTING_HALL_OPEN_MODE, mode);

        if (mode != DISPLAY_MODE_DOCKED) {
            putSystemInt(SETTING_DOCK_ORIENTATION, 0);
        }

        broadcastMode(mode, getCurrentSingleDisplay());
        return true;
    }

    public boolean switchDisplayToShow(int displayId) {
        int singleDisplay = mapDisplayToSingleDisplay(displayId);
        if (singleDisplay < 0) {
            return false;
        }

        putSystemInt(SETTING_SINGLE_DISPLAY, singleDisplay);
        putSystemInt(SETTING_USER_SINGLE_DISPLAY, singleDisplay);
        broadcastMode(getCurrentMode(), singleDisplay);
        return true;
    }

    public int switchRotationForDock(int rotation) {
        if (rotation >= 0) {
            putSystemInt(SETTING_DOCK_ORIENTATION, rotation);
            setSystemProperty(PROP_DOCK_ORIENTATION, Integer.toString(rotation));
        }

        return getSystemInt(SETTING_DOCK_ORIENTATION, getPropInt(PROP_DOCK_ORIENTATION, 0));
    }

    public ComponentName getTopActivityOnDisplay(int displayId) {
        if (displayId == 1 && getCurrentMode() != DISPLAY_MODE_DOCKED) {
            return null;
        }

        if (mContext == null) {
            return null;
        }

        ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) {
            return null;
        }

        try {
            List<ActivityManager.RunningTaskInfo> tasks = am.getRunningTasks(1);
            if (tasks != null && !tasks.isEmpty()) {
                return tasks.get(0).topActivity;
            }
        } catch (SecurityException e) {
            Log.w(TAG, "getRunningTasks denied", e);
        }

        return null;
    }

    public boolean setMainDisplayDeviceTP(int state) {
        return true;
    }

    public void setSecondaryLCDState(int state) {
        putSystemInt("zte_secondary_lcd_state", state);
    }

    public void recoverSecondaryDisplayDeviceBrightness() {
        // The vendor init script owns the actual panel backlight programming.
    }

    public int setMainDisplayDeviceBrightness(int brightness) {
        return brightness;
    }

    public int setMainDisplayDeviceBrightnessForCamera(int brightness) {
        return brightness;
    }

    public void setSecondaryDisplayDevicePowerState(int state) {
        putSystemInt("zte_secondary_display_power_state", state);
    }

    public int getHallDegree() {
        return getHallSensorStatus();
    }

    public boolean isOpen() {
        return getHallSensorStatus() == HALL_STATUS_OPEN;
    }

    private int getCurrentSingleDisplay() {
        return getSystemInt(
                SETTING_SINGLE_DISPLAY,
                getPropInt(PROP_DEFAULT_SINGLE_DISPLAY, SINGLE_DISPLAY_A));
    }

    private void broadcastMode(int mode, int singleDisplay) {
        if (mContext == null) {
            return;
        }

        Intent intent = new Intent(ACTION_MODE_SWITCHED);
        intent.putExtra(SETTING_DISPLAY_MODE, mode);
        intent.putExtra(SETTING_SINGLE_DISPLAY, singleDisplay);
        mContext.sendBroadcast(intent);

        Intent taskIntent = new Intent(ACTION_TASK_SWITCH);
        taskIntent.putExtra(SETTING_DISPLAY_MODE, mode);
        taskIntent.putExtra(SETTING_SINGLE_DISPLAY, singleDisplay);
        mContext.sendBroadcast(taskIntent);
    }

    private boolean isValidMode(int mode) {
        return mode == DISPLAY_MODE_SINGLE
                || mode == DISPLAY_MODE_ZOOM
                || mode == DISPLAY_MODE_DOCKED
                || mode == DISPLAY_MODE_MIRROR;
    }

    private int mapDisplayToSingleDisplay(int displayId) {
        if (displayId == 1 || displayId == 3) {
            return SINGLE_DISPLAY_B;
        }
        if (displayId == 0 || displayId == 2) {
            return SINGLE_DISPLAY_A;
        }
        return -1;
    }

    private int getSystemInt(String name, int fallback) {
        if (mContext == null || TextUtils.isEmpty(name)) {
            return fallback;
        }

        try {
            return Settings.System.getInt(mContext.getContentResolver(), name);
        } catch (Settings.SettingNotFoundException e) {
            return fallback;
        } catch (SecurityException e) {
            Log.w(TAG, "Unable to read setting " + name, e);
            return fallback;
        }
    }

    private void putSystemInt(String name, int value) {
        if (mContext == null || TextUtils.isEmpty(name)) {
            return;
        }

        try {
            Settings.System.putInt(mContext.getContentResolver(), name, value);
        } catch (SecurityException e) {
            Log.w(TAG, "Unable to write setting " + name, e);
        }
    }

    private int getPropInt(String name, int fallback) {
        String value = getSystemProperty(name, null);
        if (TextUtils.isEmpty(value)) {
            return fallback;
        }

        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    private String getSystemProperty(String name, String fallback) {
        try {
            Class<?> clazz = Class.forName("android.os.SystemProperties");
            Method get = clazz.getMethod("get", String.class, String.class);
            return (String) get.invoke(null, name, fallback);
        } catch (Exception e) {
            return fallback;
        }
    }

    private void setSystemProperty(String name, String value) {
        if (TextUtils.isEmpty(name) || value == null) {
            return;
        }

        try {
            Class<?> clazz = Class.forName("android.os.SystemProperties");
            Method set = clazz.getMethod("set", String.class, String.class);
            set.invoke(null, name, value);
        } catch (Exception e) {
            Log.w(TAG, "Unable to set property " + name, e);
        }
    }
}
