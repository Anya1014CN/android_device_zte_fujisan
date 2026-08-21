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
    /*
     * Camera apps briefly release one camera before acquiring the other.  Keep the
     * session snapshot during that gap so a front/back switch cannot become a new
     * session with the already-flipped panel as its restore target.
     */
    private static final long CAMERA_STOP_DELAY_MS = 2000L;
    /* Give fujisan_halld time to publish active_primary after the hinge closes. */
    private static final long FOLD_STATE_SETTLE_DELAY_MS = 300L;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Set<String> mUnavailable = new HashSet<>();
    private final Map<String, Integer> mLensFacing = new HashMap<>();

    /* Captured once for the entire camera session, never per lens switch. */
    private String mOriginalPanel;
    private int mActiveFacing = -1;
    private volatile boolean mAwaitingFold;
    private boolean mRearWideWarning;
    private boolean mPropertyCallbackRegistered;
    private Runnable mPendingPanelSwitch;
    private Runnable mPendingSessionEnd;
    private Runnable mPendingFoldCheck;
    private final Runnable mSystemPropertyChanged = () -> {
        if (mAwaitingFold) {
            mHandler.post(this::handleSystemPropertyChanged);
        }
    };

    private final CameraManager.AvailabilityCallback mAvailability =
            new CameraManager.AvailabilityCallback() {
        @Override
        public void onCameraUnavailable(String cameraId) {
            final int facing = logicalFacing(cameraId);
            Log.i(TAG, "camera unavailable id=" + cameraId + " facing=" + facing);
            /*
             * A logical camera can temporarily make its sibling unavailable too.
             * Only the first unavailable ID identifies a new active camera; a real
             * lens hand-off first empties the set and is caught by the stop debounce.
             */
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
                scheduleCameraStop();
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
            if (!mPropertyCallbackRegistered) {
                SystemProperties.addChangeCallback(mSystemPropertyChanged);
                mPropertyCallbackRegistered = true;
            }
        } catch (CameraAccessException | SecurityException ignored) {
            stopSelf();
        }
    }

    @Override
    public void onDestroy() {
        mAwaitingFold = false;
        if (mPropertyCallbackRegistered) {
            SystemProperties.removeChangeCallback(mSystemPropertyChanged);
            mPropertyCallbackRegistered = false;
        }
        cancelPendingSessionEnd();
        cancelPendingPanelSwitch();
        cancelPendingFoldCheck();
        super.onDestroy();
    }

    private void handleCameraStart(String cameraId) {
        final int facing = logicalFacing(cameraId);
        if (!isHandledFacing(facing)) {
            return;
        }

        cancelPendingSessionEnd();
        beginCameraSessionIfNeeded();
        mActiveFacing = facing;

        final String mode = displayMode();
        Log.i(TAG, "camera start id=" + cameraId + " mode=" + mode
                + " original=" + mOriginalPanel + " facing=" + facing);
        if ("zoom".equals(mode)) {
            if (facing == CameraCharacteristics.LENS_FACING_BACK && !mRearWideWarning) {
                mRearWideWarning = true;
                Toast.makeText(this, R.string.close_device, Toast.LENGTH_LONG).show();
            }
            /* The app can stay open while the user folds the device. */
            mAwaitingFold = true;
            handleSystemPropertyChanged();
            return;
        }

        mAwaitingFold = false;
        cancelPendingFoldCheck();
        switchPanelForActiveCamera();
    }

    private void beginCameraSessionIfNeeded() {
        if (mOriginalPanel != null) {
            return;
        }
        /* active_primary is always A while unfolded; use the user's preference then. */
        mOriginalPanel = "zoom".equals(displayMode()) ? preferredPanel() : panel();
        Log.i(TAG, "camera session begins, original panel=" + mOriginalPanel);
    }

    private void switchPanelForActiveCamera() {
        if (mOriginalPanel == null || !isHandledFacing(mActiveFacing)) {
            return;
        }
        if ("zoom".equals(displayMode())) {
            mAwaitingFold = true;
            handleSystemPropertyChanged();
            return;
        }

        final String current = panel();
        final String target = panelForFacing(mActiveFacing);
        cancelPendingPanelSwitch();
        Log.i(TAG, "camera panel check current=" + current + " target=" + target
                + " original=" + mOriginalPanel + " facing=" + mActiveFacing);
        if (current.equals(target)) {
            return;
        }

        Toast.makeText(this, R.string.flip_device, Toast.LENGTH_SHORT).show();
        mPendingPanelSwitch = () -> {
            mPendingPanelSwitch = null;
            if (!mUnavailable.isEmpty() && mOriginalPanel != null
                    && "single".equals(displayMode())
                    && target.equals(panelForFacing(mActiveFacing))) {
                requestPanel(target);
            }
        };
        mHandler.postDelayed(mPendingPanelSwitch, PANEL_SWITCH_DELAY_MS);
    }

    private void handleSystemPropertyChanged() {
        if (!mAwaitingFold || mOriginalPanel == null || mUnavailable.isEmpty()
                || "zoom".equals(displayMode())) {
            return;
        }
        scheduleFoldedPanelCheck();
    }

    private void scheduleFoldedPanelCheck() {
        if (mPendingFoldCheck != null || !mAwaitingFold) {
            return;
        }
        mPendingFoldCheck = () -> {
            mPendingFoldCheck = null;
            if (mOriginalPanel == null || mUnavailable.isEmpty() || !mAwaitingFold) {
                return;
            }
            if ("zoom".equals(displayMode())) {
                return;
            }
            /* Run exactly one panel check once the folded topology has settled. */
            mAwaitingFold = false;
            switchPanelForActiveCamera();
        };
        mHandler.postDelayed(mPendingFoldCheck, FOLD_STATE_SETTLE_DELAY_MS);
    }

    private void scheduleCameraStop() {
        if (mOriginalPanel == null || mPendingSessionEnd != null) {
            return;
        }
        /* Do not execute a stale switch after the app has released its camera. */
        cancelPendingPanelSwitch();
        cancelPendingFoldCheck();
        /* A lens hand-off may report an empty availability set momentarily. */
        mPendingSessionEnd = () -> {
            mPendingSessionEnd = null;
            if (!mUnavailable.isEmpty()) {
                return;
            }
            finishCameraSession();
        };
        mHandler.postDelayed(mPendingSessionEnd, CAMERA_STOP_DELAY_MS);
    }

    private void finishCameraSession() {
        mRearWideWarning = false;
        mAwaitingFold = false;
        cancelPendingFoldCheck();
        cancelPendingPanelSwitch();

        if (mOriginalPanel == null) {
            return;
        }
        final String restore = mOriginalPanel;
        mOriginalPanel = null;
        mActiveFacing = -1;
        Log.i(TAG, "camera session ends, restore panel=" + restore);

        /* A camera session that began unfolded may end before the device is folded. */
        if ("zoom".equals(displayMode())) {
            Log.i(TAG, "skip panel restore while device remains unfolded");
            return;
        }
        if (panel().equals(restore)) {
            return;
        }
        Toast.makeText(this, R.string.flip_device, Toast.LENGTH_SHORT).show();
        mPendingPanelSwitch = () -> {
            mPendingPanelSwitch = null;
            if (mUnavailable.isEmpty() && "single".equals(displayMode())) {
                requestPanel(restore);
            }
        };
        mHandler.postDelayed(mPendingPanelSwitch, CAMERA_STOP_DELAY_MS);
    }

    private void cancelPendingSessionEnd() {
        if (mPendingSessionEnd != null) {
            mHandler.removeCallbacks(mPendingSessionEnd);
            mPendingSessionEnd = null;
        }
    }

    private void cancelPendingPanelSwitch() {
        if (mPendingPanelSwitch != null) {
            mHandler.removeCallbacks(mPendingPanelSwitch);
            mPendingPanelSwitch = null;
        }
    }

    private void cancelPendingFoldCheck() {
        if (mPendingFoldCheck != null) {
            mHandler.removeCallbacks(mPendingFoldCheck);
            mPendingFoldCheck = null;
        }
    }

    private String displayMode() {
        return SystemProperties.get("vendor.fujisan.display_mode", "single");
    }

    private String panel() {
        return "b".equals(SystemProperties.get("vendor.fujisan.active_primary", "a"))
                ? "b" : "a";
    }

    private String preferredPanel() {
        return "b".equals(SystemProperties.get("persist.vendor.fujisan.primary_panel", "a"))
                ? "b" : "a";
    }

    private static String panelForFacing(int facing) {
        return facing == CameraCharacteristics.LENS_FACING_BACK ? "b" : "a";
    }

    private static boolean isHandledFacing(int facing) {
        return facing == CameraCharacteristics.LENS_FACING_BACK
                || facing == CameraCharacteristics.LENS_FACING_FRONT;
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
