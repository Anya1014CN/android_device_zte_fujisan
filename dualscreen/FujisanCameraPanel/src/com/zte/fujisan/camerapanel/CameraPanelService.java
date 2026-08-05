package com.zte.fujisan.camerapanel;

import android.app.Service;
import android.content.Intent;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemProperties;
import android.util.Log;
import android.widget.Toast;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Device-side bridge between camera availability and the existing panel HAL. */
public final class CameraPanelService extends Service {
    private static final String TAG = "FujisanCameraPanel";
    private static final String SOCKET_NAME = "fujisan_primary";
    private static final long PANEL_SWITCH_DELAY_MS = 1000L;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Set<String> mUnavailable = new HashSet<>();
    private final Map<String, Integer> mLensFacing = new HashMap<>();
    private String mOriginalPanel;
    private boolean mSwitchedForCamera;
    private boolean mRearWideWarning;
    private Runnable mPendingSwitch;

    private final CameraManager.AvailabilityCallback mAvailability =
            new CameraManager.AvailabilityCallback() {
        @Override
        public void onCameraUnavailable(String cameraId) {
            final int facing = logicalFacing(cameraId);
            Log.i(TAG, "camera unavailable id=" + cameraId + " facing=" + facing);
            if ("zoom".equals(SystemProperties.get("vendor.fujisan.display_mode", "single"))
                    && facing == CameraCharacteristics.LENS_FACING_BACK && !mRearWideWarning) {
                mRearWideWarning = true;
                Toast.makeText(CameraPanelService.this, R.string.close_device,
                        Toast.LENGTH_LONG).show();
            }
            if (!mUnavailable.add(cameraId) || mUnavailable.size() != 1) {
                return;
            }
            handleCameraStart(cameraId);
        }

        @Override
        public void onCameraAvailable(String cameraId) {
            Log.i(TAG, "camera available id=" + cameraId);
            mUnavailable.remove(cameraId);
            if (mUnavailable.isEmpty()) {
                handleCameraStop();
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        final CameraManager manager = getSystemService(CameraManager.class);
        try {
            for (String id : manager.getCameraIdList()) {
                final Integer facing = manager.getCameraCharacteristics(id).get(
                        CameraCharacteristics.LENS_FACING);
                if (facing != null) {
                    mLensFacing.put(id, facing);
                }
            }
            manager.registerAvailabilityCallback(mAvailability, mHandler);
        } catch (CameraAccessException | SecurityException ignored) {
            stopSelf();
        }
    }

    private void handleCameraStart(String cameraId) {
        final int facing = logicalFacing(cameraId);
        final String mode = SystemProperties.get("vendor.fujisan.display_mode", "single");
        if ("zoom".equals(mode)) {
            if (facing == CameraCharacteristics.LENS_FACING_BACK && !mRearWideWarning) {
                mRearWideWarning = true;
                Toast.makeText(this, R.string.close_device, Toast.LENGTH_LONG).show();
            }
            return;
        }

        final String current = panel();
        final String target = facing == CameraCharacteristics.LENS_FACING_BACK ? "b" : "a";
        Log.i(TAG, "camera start id=" + cameraId + " mode=" + mode
                + " current=" + current + " target=" + target);
        if (current.equals(target)) {
            return;
        }
        mOriginalPanel = current;
        mSwitchedForCamera = true;
        Toast.makeText(this, R.string.flip_device, Toast.LENGTH_SHORT).show();
        mPendingSwitch = () -> {
            mPendingSwitch = null;
            if (mSwitchedForCamera) {
                requestPanel(target);
            }
        };
        mHandler.postDelayed(mPendingSwitch, PANEL_SWITCH_DELAY_MS);
    }

    private void handleCameraStop() {
        mRearWideWarning = false;
        if (!mSwitchedForCamera || mOriginalPanel == null) {
            return;
        }
        if (mPendingSwitch != null) {
            mHandler.removeCallbacks(mPendingSwitch);
            mPendingSwitch = null;
        }
        final String restore = mOriginalPanel;
        mSwitchedForCamera = false;
        mOriginalPanel = null;
        requestPanel(restore);
    }

    private String panel() {
        return "b".equals(SystemProperties.get("vendor.fujisan.active_primary", "a"))
                ? "b" : "a";
    }

    private void requestPanel(String target) {
        try (android.net.LocalSocket socket = new android.net.LocalSocket()) {
            socket.connect(new android.net.LocalSocketAddress(
                    SOCKET_NAME, android.net.LocalSocketAddress.Namespace.RESERVED));
            socket.getOutputStream().write(("set " + target + "\n").getBytes(StandardCharsets.US_ASCII));
            socket.getOutputStream().flush();
            final byte[] response = new byte[32];
            final int count = socket.getInputStream().read(response);
            Log.i(TAG, "panel request target=" + target + " response="
                    + (count > 0 ? new String(response, 0, count, StandardCharsets.US_ASCII).trim()
                                  : "empty"));
        } catch (IOException ignored) {
            Log.e(TAG, "panel request failed target=" + target, ignored);
        }
    }

    private int logicalFacing(String cameraId) {
        if ("0".equals(cameraId)) {
            return CameraCharacteristics.LENS_FACING_BACK;
        }
        if ("1".equals(cameraId)) {
            return CameraCharacteristics.LENS_FACING_FRONT;
        }
        return mLensFacing.containsKey(cameraId) ? mLensFacing.get(cameraId) : -1;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
