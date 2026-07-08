package com.zte.duallcd;

import android.app.Service;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.provider.Settings;

import com.zte.dualLcdManager.DisplayModeManager;

public class DisplayModeBootstrapService extends Service {
    private static final String[] OBSERVED_SYSTEM_SETTINGS = new String[] {
            "display_mode",
            "displayid_single_mode",
            "user_choose_displayid_single_mode",
            "hallC_display_mode",
            "dual_screen_dock_mode_screen_orientation_mode",
    };

    private DisplayModeManager mDisplayModeManager;
    private ContentObserver mSettingsObserver;

    public static void start(Context context) {
        Intent intent = new Intent(context, DisplayModeBootstrapService.class);
        context.startService(intent);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mDisplayModeManager = DisplayModeManager.getInstance(this);
        mSettingsObserver = new ContentObserver(new Handler(Looper.getMainLooper())) {
            @Override
            public void onChange(boolean selfChange) {
                syncNow();
            }

            @Override
            public void onChange(boolean selfChange, Uri uri) {
                syncNow();
            }
        };
        registerObservers();
        syncNow();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        syncNow();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        unregisterObservers();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void registerObservers() {
        ContentResolver resolver = getContentResolver();
        for (String setting : OBSERVED_SYSTEM_SETTINGS) {
            resolver.registerContentObserver(
                    Settings.System.getUriFor(setting),
                    false,
                    mSettingsObserver);
        }
    }

    private void unregisterObservers() {
        if (mSettingsObserver != null) {
            getContentResolver().unregisterContentObserver(mSettingsObserver);
        }
    }

    private void syncNow() {
        if (mDisplayModeManager != null) {
            mDisplayModeManager.refreshCurrentState();
        }
    }
}
