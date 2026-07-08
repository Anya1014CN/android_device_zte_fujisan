package com.zte.duallcd;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import com.zte.dualLcdManager.DisplayModeManager;

public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "DualLcdBootReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        try {
            DisplayModeManager.getInstance(context).refreshCurrentState();
        } catch (Throwable t) {
            Log.e(TAG, "Failed to refresh dual-screen state", t);
        }
    }
}
