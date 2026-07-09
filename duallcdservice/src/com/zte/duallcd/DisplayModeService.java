package com.zte.duallcd;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.IBinder;
import android.provider.Settings;
import android.util.Log;

public class DisplayModeService extends Service {
    private static final String TAG = "DisplayModeService";
    private static final int HALL_SENSOR_TYPE = 65537;
    private static final int HALL_OPEN = 3;
    private static final int HALL_CLOSED = 1;
    private static final int DISPLAY_MODE_SINGLE = 1;
    private static final int DISPLAY_MODE_ZOOM = 2;
    private static final int DISPLAY_MODE_DOCKED = 4;
    private static final int DISPLAY_MODE_MIRROR = 8;

    private SensorManager mSensorManager;
    private Sensor mHallSensor;
    private int mCurrentMode = DISPLAY_MODE_ZOOM;
    private int mSingleDisplay = 0;

    private final SensorEventListener mHallListener = new SensorEventListener() {
        @Override
        public void onSensorChanged(SensorEvent event) {
            if (event.values.length > 0) {
                int status = Math.round(event.values[0]);
                Log.d(TAG, "Hall sensor: " + status);
                handleHallChange(status);
            }
        }
        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {}
    };

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "DisplayModeService starting");

        mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
        if (mSensorManager != null) {
            mHallSensor = mSensorManager.getDefaultSensor(HALL_SENSOR_TYPE);
            if (mHallSensor != null) {
                mSensorManager.registerListener(mHallListener, mHallSensor,
                        SensorManager.SENSOR_DELAY_NORMAL);
                Log.i(TAG, "Hall sensor registered");
            }
        }

        restoreState();
        applyCurrentMode();

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_BOOT_COMPLETED);
        registerReceiver(mBootReceiver, filter);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private final BroadcastReceiver mBootReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            restoreState();
            applyCurrentMode();
        }
    };

    private void restoreState() {
        try {
            mCurrentMode = Settings.System.getInt(getContentResolver(), "display_mode");
        } catch (Settings.SettingNotFoundException e) {
            mCurrentMode = DISPLAY_MODE_ZOOM;
        }
        try {
            mSingleDisplay = Settings.System.getInt(getContentResolver(), "displayid_single_mode");
        } catch (Settings.SettingNotFoundException e) {
            mSingleDisplay = 0;
        }
    }

    private void handleHallChange(int hallStatus) {
        if (hallStatus == HALL_CLOSED) {
            setMode(DISPLAY_MODE_SINGLE);
        } else if (hallStatus == HALL_OPEN) {
            try {
                int openMode = Settings.System.getInt(getContentResolver(), "hallC_display_mode");
                setMode(openMode);
            } catch (Settings.SettingNotFoundException e) {
                setMode(DISPLAY_MODE_ZOOM);
            }
        }
    }

    private void applyCurrentMode() {
        setMode(mCurrentMode);
    }

    private void setMode(int mode) {
        mCurrentMode = mode;
        boolean secondaryOn = (mode == DISPLAY_MODE_ZOOM ||
                               mode == DISPLAY_MODE_DOCKED ||
                               mode == DISPLAY_MODE_MIRROR ||
                               (mode == DISPLAY_MODE_SINGLE && mSingleDisplay == 1));

        setProp("persist.vendor.fujisan.force_dual_screen", secondaryOn ? "1" : "0");

        if (secondaryOn) {
            enableSecondaryPanel();
        } else {
            disableSecondaryPanel();
        }

        Log.i(TAG, "Display mode set to " + mode + ", secondary=" + secondaryOn);
    }

    private void enableSecondaryPanel() {
        writeSysfs("/sys/class/leds/lcd-backlight-2/brightness", "180");
        writeSysfs("/sys/class/graphics/fb1/blank", "0");
        Log.i(TAG, "Secondary panel enabled");
    }

    private void disableSecondaryPanel() {
        writeSysfs("/sys/class/leds/lcd-backlight-2/brightness", "0");
        writeSysfs("/sys/class/graphics/fb1/blank", "4");
        Log.i(TAG, "Secondary panel disabled");
    }

    private void setProp(String key, String value) {
        try {
            Class<?> c = Class.forName("android.os.SystemProperties");
            c.getMethod("set", String.class, String.class).invoke(null, key, value);
        } catch (Exception e) {
            Log.w(TAG, "setProp failed: " + key, e);
        }
    }

    private void writeSysfs(String path, String value) {
        try {
            java.io.FileWriter fw = new java.io.FileWriter(path);
            fw.write(value);
            fw.close();
        } catch (Exception e) {
            Log.w(TAG, "writeSysfs failed: " + path, e);
        }
    }
}
