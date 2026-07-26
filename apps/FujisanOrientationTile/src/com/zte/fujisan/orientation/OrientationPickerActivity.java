/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package com.zte.fujisan.orientation;

import android.app.Activity;
import android.app.AlertDialog;
import android.hardware.devicestate.DeviceStateManager;
import android.os.Bundle;
import android.widget.Toast;

/** Small chooser launched by the QS tile; it never controls display topology. */
public final class OrientationPickerActivity extends Activity {
    private static final int DEVICE_STATE_OPENED = 1;

    private DeviceStateManager mDeviceStateManager;
    private DeviceStateManager.DeviceStateCallback mDeviceStateCallback;
    private boolean mDialogShown;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDeviceStateManager = getSystemService(DeviceStateManager.class);
        mDeviceStateCallback = state -> {
            if (state != DEVICE_STATE_OPENED) {
                Toast.makeText(this, R.string.expand_device, Toast.LENGTH_SHORT).show();
                finish();
            } else if (!mDialogShown) {
                showPicker();
            }
        };
        // The initial callback is guaranteed by DeviceStateManager, so no polling or stale
        // posture read is needed before showing the chooser.
        mDeviceStateManager.registerCallback(getMainExecutor(), mDeviceStateCallback);
    }

    @Override
    protected void onDestroy() {
        if (mDeviceStateCallback != null) {
            mDeviceStateManager.unregisterCallback(mDeviceStateCallback);
            mDeviceStateCallback = null;
        }
        super.onDestroy();
    }

    private void showPicker() {
        mDialogShown = true;
        final CharSequence[] modes = {
                getString(R.string.auto_rotate),
                getString(R.string.force_landscape),
                getString(R.string.force_portrait),
        };
        final int selected = OrientationController.getMode(this);
        new AlertDialog.Builder(this)
                .setTitle(R.string.tile_title)
                .setSingleChoiceItems(modes, selected, (dialog, which) -> {
                    OrientationController.setMode(this, which);
                    dialog.dismiss();
                    finish();
                })
                .setOnCancelListener(dialog -> finish())
                .show();
    }
}
