package com.zte.duallcd;

import android.app.Application;
import android.content.ContentResolver;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;

import com.zte.dualLcdManager.DisplayModeManager;

public class DualLcdApplication extends Application {
    private static final String TAG = "DualLcdApplication";
    private static final String[] OBSERVED_SYSTEM_SETTINGS = new String[] {
            "display_mode",
            "displayid_single_mode",
            "user_choose_displayid_single_mode",
            "hallC_display_mode",
            "dual_screen_dock_mode_screen_orientation_mode",
    };

    private static DisplayModeManager sDisplayModeManager;
    private static ContentObserver sSettingsObserver;
    private static boolean sInitialized;

    @Override
    public void onCreate() {
        super.onCreate();
        ensureInitialized(this);
    }

    public static synchronized void ensureInitialized(Application application) {
        if (sInitialized || application == null) {
            if (sDisplayModeManager != null) {
                syncNow();
            }
            return;
        }

        try {
            sDisplayModeManager = DisplayModeManager.getInstance(application);
            sSettingsObserver = new ContentObserver(new Handler(Looper.getMainLooper())) {
                @Override
                public void onChange(boolean selfChange) {
                    syncNow();
                }

                @Override
                public void onChange(boolean selfChange, Uri uri) {
                    syncNow();
                }
            };

            ContentResolver resolver = application.getContentResolver();
            for (String setting : OBSERVED_SYSTEM_SETTINGS) {
                resolver.registerContentObserver(
                        Settings.System.getUriFor(setting),
                        false,
                        sSettingsObserver);
            }

            sInitialized = true;
            syncNow();
        } catch (Throwable t) {
            Log.e(TAG, "Failed to initialize dual-screen state bridge", t);
        }
    }

    public static synchronized void ensureInitialized(android.content.Context context) {
        if (context instanceof Application) {
            ensureInitialized((Application) context);
            return;
        }

        Application application = context != null
                ? (Application) context.getApplicationContext()
                : null;
        ensureInitialized(application);
    }

    public static synchronized void syncNow() {
        if (sDisplayModeManager == null) {
            return;
        }

        try {
            sDisplayModeManager.refreshCurrentState();
        } catch (Throwable t) {
            Log.e(TAG, "Failed to sync dual-screen state", t);
        }
    }
}
