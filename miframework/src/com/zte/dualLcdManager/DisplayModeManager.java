package com.zte.dualLcdManager;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
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
    private static final String PROP_SECONDARY_BACKLIGHT = "persist.vendor.fujisan.secondary_backlight";
    private static final String PROP_HALL_STATUS = "persist.sys.zte.hallStatus";
    private static final String PROP_DOCK_ORIENTATION = "persist.sys.zte.dockori";
    private static final String PROP_FORCE_DUAL = "persist.vendor.fujisan.force_dual_screen";
    private static final String SETTING_DISPLAY_MODE = "display_mode";
    private static final String SETTING_SINGLE_DISPLAY = "displayid_single_mode";
    private static final String SETTING_USER_SINGLE_DISPLAY = "user_choose_displayid_single_mode";
    private static final String SETTING_HALL_OPEN_MODE = "hallC_display_mode";
    private static final String SETTING_DOCK_ORIENTATION = "dual_screen_dock_mode_screen_orientation_mode";
    private static final String SETTING_SECONDARY_LCD_STATE = "zte_secondary_lcd_state";
    private static final String SETTING_SECONDARY_DISPLAY_POWER = "zte_secondary_display_power_state";
    private static final String SETTING_MAIN_BRIGHTNESS = "screen_brightness";
    private static final String CAMERA_PACKAGE = "com.zte.camera";
    private static final int HALL_SENSOR_TYPE = 65537;
    private static final int HALL_STATUS_OPEN = 3;
    private static final int HALL_STATUS_CLOSED = 1;
    private static final int SINGLE_DISPLAY_A = 0;
    private static final int SINGLE_DISPLAY_B = 1;
    private static final int DISPLAY_STATE_OFF = 0;
    private static final int DISPLAY_STATE_ON = 2;

    private static DisplayModeManager sInstance;

    private final Context mContext;
    private int mHallStatus = -1;
    private SensorManager mSensorManager;
    private SensorEventListener mHallListener;

    private DisplayModeManager(Context context) {
        mContext = context != null ? context.getApplicationContext() : null;
        initHallListener();
    }

    public static synchronized DisplayModeManager getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new DisplayModeManager(context);
        }
        return sInstance;
    }

    public int getCurrentMode() {
        int hallStatus = getHallSensorStatus();
        int fallback = hallStatus == HALL_STATUS_OPEN
                ? getSystemInt(SETTING_HALL_OPEN_MODE, DISPLAY_MODE_ZOOM)
                : DISPLAY_MODE_SINGLE;
        int mode = getSystemInt(SETTING_DISPLAY_MODE, getPropInt(PROP_DEFAULT_MODE, fallback));

        if (hallStatus != HALL_STATUS_OPEN && mode != DISPLAY_MODE_SINGLE) {
            return DISPLAY_MODE_SINGLE;
        }

        return mode;
    }

    public int getFocusDisplayId() {
        if (getCurrentMode() == DISPLAY_MODE_SINGLE && getCurrentSingleDisplay() == SINGLE_DISPLAY_B) {
            return 1;
        }
        return 0;
    }

    public int getHallSensorStatus() {
        if (mHallStatus > 0) {
            return mHallStatus;
        }
        return getPropInt(PROP_HALL_STATUS, 1);
    }

    public boolean switchMode(int mode) {
        if (!isValidMode(mode)) {
            return false;
        }

        putSystemInt(SETTING_DISPLAY_MODE, mode);
        putSystemInt(SETTING_HALL_OPEN_MODE, mode);
        setSystemProperty(PROP_DEFAULT_MODE, Integer.toString(mode));

        if (mode != DISPLAY_MODE_DOCKED) {
            putSystemInt(SETTING_DOCK_ORIENTATION, 0);
            setSystemProperty(PROP_DOCK_ORIENTATION, "0");
        }

        syncSecondaryState(mode, getCurrentSingleDisplay());

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
        setSystemProperty(PROP_DEFAULT_SINGLE_DISPLAY, Integer.toString(singleDisplay));
        syncSecondaryState(getCurrentMode(), singleDisplay);
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
        if (displayId == 1) {
            if (getCurrentMode() == DISPLAY_MODE_DOCKED || getCurrentSingleDisplay() == SINGLE_DISPLAY_B) {
                return new ComponentName(CAMERA_PACKAGE, "com.zte.camera.CameraActivity");
            }
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
        putSystemInt(SETTING_SECONDARY_LCD_STATE, state);
        syncSecondaryPowerState(state);
    }

    public void recoverSecondaryDisplayDeviceBrightness() {
        setSystemProperty(PROP_SECONDARY_BACKLIGHT,
                Integer.toString(getPropInt(PROP_SECONDARY_BACKLIGHT, 60)));
    }

    public int setMainDisplayDeviceBrightness(int brightness) {
        putSystemInt(SETTING_MAIN_BRIGHTNESS, brightness);
        return brightness;
    }

    public int setMainDisplayDeviceBrightnessForCamera(int brightness) {
        putSystemInt(SETTING_MAIN_BRIGHTNESS, brightness);
        return brightness;
    }

    public void setSecondaryDisplayDevicePowerState(int state) {
        putSystemInt(SETTING_SECONDARY_DISPLAY_POWER, state);
        syncSecondaryPowerState(state);
    }

    public int getHallDegree() {
        return getHallSensorStatus();
    }

    public boolean isOpen() {
        return getHallSensorStatus() == HALL_STATUS_OPEN;
    }

    private void initHallListener() {
        if (mContext == null) {
            return;
        }

        try {
            mSensorManager = (SensorManager) mContext.getSystemService(Context.SENSOR_SERVICE);
            if (mSensorManager == null) {
                return;
            }

            Sensor hallSensor = mSensorManager.getDefaultSensor(HALL_SENSOR_TYPE);
            if (hallSensor == null) {
                return;
            }

            mHallListener = new SensorEventListener() {
                @Override
                public void onSensorChanged(SensorEvent event) {
                    if (event == null || event.values == null || event.values.length == 0) {
                        return;
                    }

                    int hallStatus = Math.round(event.values[0]);
                    if (hallStatus <= 0 || hallStatus == mHallStatus) {
                        return;
                    }

                    mHallStatus = hallStatus;
                    setSystemProperty(PROP_HALL_STATUS, Integer.toString(hallStatus));

                    if (hallStatus == HALL_STATUS_CLOSED) {
                        putSystemInt(SETTING_DISPLAY_MODE, DISPLAY_MODE_SINGLE);
                        setSystemProperty(PROP_DEFAULT_MODE, Integer.toString(DISPLAY_MODE_SINGLE));
                        syncSecondaryState(DISPLAY_MODE_SINGLE, getCurrentSingleDisplay());
                    } else if (hallStatus == HALL_STATUS_OPEN) {
                        int restoredMode = getSystemInt(SETTING_HALL_OPEN_MODE, DISPLAY_MODE_ZOOM);
                        putSystemInt(SETTING_DISPLAY_MODE, restoredMode);
                        setSystemProperty(PROP_DEFAULT_MODE, Integer.toString(restoredMode));
                        syncSecondaryState(restoredMode, getCurrentSingleDisplay());
                    }
                }

                @Override
                public void onAccuracyChanged(Sensor sensor, int accuracy) {
                }
            };

            mSensorManager.registerListener(mHallListener, hallSensor, SensorManager.SENSOR_DELAY_NORMAL);
        } catch (Exception e) {
            Log.w(TAG, "Unable to register hall sensor listener", e);
        }
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

    private void syncSecondaryState(int mode, int singleDisplay) {
        boolean secondaryOn = mode == DISPLAY_MODE_ZOOM
                || mode == DISPLAY_MODE_DOCKED
                || mode == DISPLAY_MODE_MIRROR
                || (mode == DISPLAY_MODE_SINGLE && singleDisplay == SINGLE_DISPLAY_B);
        setSystemProperty(PROP_FORCE_DUAL, secondaryOn ? "1" : "0");
        syncSecondaryPowerState(secondaryOn ? DISPLAY_STATE_ON : DISPLAY_STATE_OFF);
    }

    private void syncSecondaryPowerState(int state) {
        putSystemInt(SETTING_SECONDARY_LCD_STATE, state);
        putSystemInt(SETTING_SECONDARY_DISPLAY_POWER, state);
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
