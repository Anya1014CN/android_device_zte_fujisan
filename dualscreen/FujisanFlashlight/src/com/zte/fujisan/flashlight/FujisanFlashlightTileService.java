package com.zte.fujisan.flashlight;

import android.app.AlertDialog;
import android.content.Context;
import android.content.res.Resources;
import android.content.pm.PackageManager;
import android.graphics.drawable.Icon;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemProperties;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.util.Log;
import android.view.ContextThemeWrapper;

/** A device-side replacement for the stock flashlight tile. */
public final class FujisanFlashlightTileService extends TileService {
    private static final String TAG = "FujisanFlashlight";

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private CameraManager mCameraManager;
    private String mCameraId;
    private boolean mListening;
    private boolean mTorchAvailable;
    private boolean mTorchEnabled;
    private boolean mCameraInUse;
    private boolean mWarningShowing;
    private Resources mSystemUiResources;

    private final CameraManager.TorchCallback mTorchCallback = new CameraManager.TorchCallback() {
        @Override
        public void onTorchModeUnavailable(String cameraId) {
            if (!cameraId.equals(mCameraId)) {
                return;
            }
            mTorchAvailable = false;
            mTorchEnabled = false;
            mCameraInUse = true;
            refreshTile();
        }

        @Override
        public void onTorchModeChanged(String cameraId, boolean enabled) {
            if (!cameraId.equals(mCameraId)) {
                return;
            }
            mTorchAvailable = true;
            mTorchEnabled = enabled;
            mCameraInUse = false;
            refreshTile();
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        mCameraManager = getSystemService(CameraManager.class);
        mCameraId = findBackFlashCamera();
        mTorchAvailable = mCameraId != null;
        try {
            mSystemUiResources = createPackageContext("com.android.systemui",
                    Context.CONTEXT_IGNORE_SECURITY).getResources();
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "SystemUI resources unavailable", e);
        }
    }

    @Override
    public void onStartListening() {
        super.onStartListening();
        if (mCameraId != null && !mListening) {
            try {
                mCameraManager.registerTorchCallback(mTorchCallback, mMainHandler);
                mListening = true;
            } catch (SecurityException e) {
                Log.e(TAG, "cannot observe torch state", e);
                mTorchAvailable = false;
            }
        }
        refreshTile();
    }

    @Override
    public void onStopListening() {
        if (mListening) {
            mCameraManager.unregisterTorchCallback(mTorchCallback);
            mListening = false;
        }
        super.onStopListening();
    }

    @Override
    public void onClick() {
        if (mCameraId == null || !mTorchAvailable || mWarningShowing) {
            return;
        }
        if (mTorchEnabled || isBPrimaryWhileFolded()) {
            setTorchEnabled(!mTorchEnabled);
            return;
        }
        showEnableWarning();
    }

    private void showEnableWarning() {
        mWarningShowing = true;
        final AlertDialog dialog = new AlertDialog.Builder(
                new ContextThemeWrapper(this, R.style.Theme_FujisanFlashlight_Dialog_Alert))
                .setMessage(getString(R.string.warning_message))
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(android.R.string.ok, (unused, which) -> setTorchEnabled(true))
                .create();
        dialog.setOnDismissListener(unused -> mWarningShowing = false);
        showDialog(dialog);
    }

    private void setTorchEnabled(boolean enabled) {
        try {
            mCameraManager.setTorchMode(mCameraId, enabled);
        } catch (CameraAccessException | IllegalArgumentException | SecurityException e) {
            Log.w(TAG, "cannot change torch mode", e);
            mTorchAvailable = false;
            mTorchEnabled = false;
            mCameraInUse = false;
            refreshTile();
        }
    }

    private String findBackFlashCamera() {
        try {
            for (String cameraId : mCameraManager.getCameraIdList()) {
                final CameraCharacteristics characteristics =
                        mCameraManager.getCameraCharacteristics(cameraId);
                final Boolean flashAvailable = characteristics.get(
                        CameraCharacteristics.FLASH_INFO_AVAILABLE);
                final Integer lensFacing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (Boolean.TRUE.equals(flashAvailable)
                        && lensFacing != null
                        && lensFacing == CameraCharacteristics.LENS_FACING_BACK) {
                    return cameraId;
                }
            }
        } catch (CameraAccessException | SecurityException e) {
            Log.e(TAG, "cannot find flashlight camera", e);
        }
        return null;
    }

    private boolean isBPrimaryWhileFolded() {
        return "single".equals(SystemProperties.get("vendor.fujisan.display_mode", "single"))
                && "b".equals(SystemProperties.get("vendor.fujisan.active_primary", "a"));
    }

    private void refreshTile() {
        final Tile tile = getQsTile();
        if (tile == null) {
            return;
        }
        final boolean unavailable = !mTorchAvailable;
        tile.setLabel(systemUiString("quick_settings_flashlight_label", R.string.tile_label));
        tile.setSubtitle(unavailable && mCameraInUse
                ? systemUiString("quick_settings_flashlight_camera_in_use", 0) : "");
        tile.setContentDescription(systemUiString(
                "quick_settings_flashlight_label", R.string.tile_label));
        tile.setStateDescription(unavailable && mCameraInUse
                ? systemUiString("quick_settings_flashlight_camera_in_use", 0)
                : systemUiState(mTorchEnabled ? 2 : 1));
        tile.setIcon(systemFlashlightIcon());
        tile.setState(mTorchAvailable
                ? (mTorchEnabled ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE)
                : Tile.STATE_UNAVAILABLE);
        tile.updateTile();
    }

    private Icon systemFlashlightIcon() {
        return Icon.createWithResource("android", com.android.internal.R.drawable.ic_qs_flashlight);
    }

    private String systemUiState(int index) {
        if (mSystemUiResources != null) {
            final int id = mSystemUiResources.getIdentifier(
                    "tile_states_flashlight", "array", "com.android.systemui");
            if (id != 0) {
                final String[] states = mSystemUiResources.getStringArray(id);
                if (index >= 0 && index < states.length) {
                    return states[index];
                }
            }
        }
        return "";
    }

    private String systemUiString(String name, int fallbackId) {
        if (mSystemUiResources != null) {
            final int id = mSystemUiResources.getIdentifier(name, "string", "com.android.systemui");
            if (id != 0) {
                return mSystemUiResources.getString(id);
            }
        }
        return fallbackId != 0 ? getString(fallbackId) : null;
    }
}
