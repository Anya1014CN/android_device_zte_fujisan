/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package com.zte.fujisan.orientation;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.RemoteException;
import android.provider.Settings;
import android.util.Log;
import android.view.Display;
import android.view.IWindowManager;
import android.view.Surface;
import android.view.WindowManagerGlobal;

/** Owns only the orientation override installed by the Fujisan QS tile. */
final class OrientationController {
    static final int MODE_AUTO = 0;
    static final int MODE_LANDSCAPE = 1;
    static final int MODE_PORTRAIT = 2;

    private static final String TAG = "FujisanOrientation";
    private static final String PREFS = "orientation";
    private static final String KEY_MODE = "mode";
    private static final String KEY_OVERRIDE_ACTIVE = "override_active";
    private static final String KEY_PREVIOUS_AUTO_ROTATION = "previous_auto_rotation";

    private OrientationController() {}

    static int getMode(Context context) {
        return prefs(context).getInt(KEY_MODE, MODE_AUTO);
    }

    static void setMode(Context context, int mode) {
        if (mode < MODE_AUTO || mode > MODE_PORTRAIT) {
            throw new IllegalArgumentException("Unknown orientation mode " + mode);
        }

        final SharedPreferences preferences = prefs(context);
        final IWindowManager windowManager = WindowManagerGlobal.getWindowManagerService();
        try {
            if (mode == MODE_AUTO) {
                // setIgnoreOrientationRequest() is not persisted.  Releasing it first lets
                // normal application orientation requests work again.
                windowManager.setIgnoreOrientationRequest(Display.DEFAULT_DISPLAY, false);
                windowManager.thawRotation();

                // freezeRotation() changes ACCELEROMETER_ROTATION as a side effect.  Restore
                // exactly the value that existed before this tile took control, rather than
                // changing the user's own auto-rotate preference.
                if (preferences.getBoolean(KEY_OVERRIDE_ACTIVE, false)) {
                    final int previous = preferences.getInt(KEY_PREVIOUS_AUTO_ROTATION, 1);
                    if (!Settings.System.putInt(context.getContentResolver(),
                            Settings.System.ACCELEROMETER_ROTATION, previous)) {
                        Log.w(TAG, "Unable to restore the user's auto-rotate setting");
                    }
                }
                preferences.edit()
                        .putInt(KEY_MODE, MODE_AUTO)
                        .putBoolean(KEY_OVERRIDE_ACTIVE, false)
                        .apply();
                return;
            }

            if (!preferences.getBoolean(KEY_OVERRIDE_ACTIVE, false)) {
                final int previous = Settings.System.getInt(context.getContentResolver(),
                        Settings.System.ACCELEROMETER_ROTATION, 1);
                preferences.edit()
                        .putInt(KEY_PREVIOUS_AUTO_ROTATION, previous)
                        .putBoolean(KEY_OVERRIDE_ACTIVE, true)
                        .apply();
            }

            // Fujisan's opened logical display is naturally 2160x1920.  ROTATION_0 is its
            // horizontal orientation; rotating clockwise creates a 1920x2160 portrait UI.
            final int rotation = mode == MODE_LANDSCAPE
                    ? Surface.ROTATION_0 : Surface.ROTATION_90;
            windowManager.setIgnoreOrientationRequest(Display.DEFAULT_DISPLAY, true);
            windowManager.freezeRotation(rotation);
            preferences.edit().putInt(KEY_MODE, mode).apply();
        } catch (RemoteException e) {
            Log.e(TAG, "Could not change display orientation", e);
        }
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
