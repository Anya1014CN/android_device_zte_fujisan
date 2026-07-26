/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
package com.zte.fujisan.orientation;

import android.content.Intent;
import android.graphics.drawable.Icon;
import android.hardware.devicestate.DeviceStateManager;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.widget.Toast;

/** QS entry point for the opened Fujisan large display orientation policy. */
public final class FujisanOrientationTile extends TileService {
    // Matches configs/devicestate/device_state_configuration.xml.
    private static final int DEVICE_STATE_OPENED = 1;

    private boolean mOpened;
    private DeviceStateManager mDeviceStateManager;
    private DeviceStateManager.DeviceStateCallback mDeviceStateCallback;

    @Override
    public void onStartListening() {
        super.onStartListening();
        if (mDeviceStateCallback == null) {
            mDeviceStateManager = getSystemService(DeviceStateManager.class);
            mDeviceStateCallback = state -> {
                mOpened = state == DEVICE_STATE_OPENED;
                refreshTile();
            };
            mDeviceStateManager.registerCallback(getMainExecutor(), mDeviceStateCallback);
        }
        refreshTile();
    }

    @Override
    public void onStopListening() {
        if (mDeviceStateCallback != null) {
            mDeviceStateManager.unregisterCallback(mDeviceStateCallback);
            mDeviceStateCallback = null;
            mDeviceStateManager = null;
        }
        super.onStopListening();
    }

    @Override
    public void onClick() {
        if (!mOpened) {
            Toast.makeText(this, R.string.expand_device, Toast.LENGTH_SHORT).show();
            return;
        }
        startActivityAndCollapse(new Intent(this, OrientationPickerActivity.class));
    }

    private void refreshTile() {
        final Tile tile = getQsTile();
        if (tile == null) {
            return;
        }

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_orientation));
        tile.setLabel(getString(R.string.tile_title));
        if (!mOpened) {
            tile.setSubtitle(getString(R.string.expand_device));
            tile.setState(Tile.STATE_UNAVAILABLE);
        } else {
            final int mode = OrientationController.getMode(this);
            tile.setSubtitle(getModeLabel(mode));
            tile.setState(mode == OrientationController.MODE_AUTO
                    ? Tile.STATE_INACTIVE : Tile.STATE_ACTIVE);
        }
        tile.updateTile();
    }

    private String getModeLabel(int mode) {
        switch (mode) {
            case OrientationController.MODE_LANDSCAPE:
                return getString(R.string.force_landscape);
            case OrientationController.MODE_PORTRAIT:
                return getString(R.string.force_portrait);
            case OrientationController.MODE_AUTO:
            default:
                return getString(R.string.auto_rotate);
        }
    }
}
