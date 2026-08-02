package com.zte.fujisan.windowinfoprobe;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.widget.TextView;

import androidx.window.sidecar.SidecarDeviceState;
import androidx.window.sidecar.SidecarDisplayFeature;
import androidx.window.sidecar.SidecarInterface;
import androidx.window.sidecar.SidecarProvider;
import androidx.window.sidecar.SidecarWindowLayoutInfo;

import java.util.Collections;
import java.util.List;

/** Foreground-only diagnostic for the source-built AOSP WindowManager Sidecar library. */
public final class MainActivity extends Activity {
    private static final String TAG = "FujisanWindowProbe";

    private TextView mOutput;
    private SidecarInterface mSidecar;
    private IBinder mWindowToken;

    private final SidecarInterface.SidecarCallback mCallback =
            new SidecarInterface.SidecarCallback() {
                @Override
                public void onDeviceStateChanged(SidecarDeviceState newDeviceState) {
                    report("device-state callback", newDeviceState, getWindowLayoutInfo());
                }

                @Override
                public void onWindowLayoutChanged(IBinder windowToken,
                        SidecarWindowLayoutInfo newLayout) {
                    if (windowToken == mWindowToken) {
                        report("layout callback", getDeviceState(), newLayout);
                    }
                }
            };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mOutput = new TextView(this);
        mOutput.setPadding(32, 32, 32, 32);
        mOutput.setTextSize(16);
        mOutput.setText("Waiting for Sidecar window token…");
        setContentView(mOutput);
    }

    @Override
    public void onPostResume() {
        super.onPostResume();
        mOutput.post(this::connectSidecar);
    }

    @Override
    protected void onPause() {
        if (mSidecar != null) {
            if (mWindowToken != null) {
                mSidecar.onWindowLayoutChangeListenerRemoved(mWindowToken);
            }
            mSidecar.onDeviceStateListenersChanged(false);
        }
        mWindowToken = null;
        super.onPause();
    }

    private void connectSidecar() {
        // Sidecar's ActivityThread lookup is keyed by the Activity window token,
        // not the ViewRoot token returned by DecorView.getWindowToken().
        mWindowToken = getWindow().getAttributes().token;
        if (mWindowToken == null) {
            mOutput.postDelayed(this::connectSidecar, 100);
            return;
        }

        mSidecar = SidecarProvider.getSidecarImpl(getApplicationContext());
        if (mSidecar == null) {
            reportText("SidecarProvider returned null");
            return;
        }

        mSidecar.setSidecarCallback(mCallback);
        mSidecar.onDeviceStateListenersChanged(true);
        mSidecar.onWindowLayoutChangeListenerAdded(mWindowToken);
        report("initial", getDeviceState(), getWindowLayoutInfo());
    }

    private SidecarDeviceState getDeviceState() {
        return mSidecar == null ? null : mSidecar.getDeviceState();
    }

    private SidecarWindowLayoutInfo getWindowLayoutInfo() {
        return mSidecar == null || mWindowToken == null
                ? null : mSidecar.getWindowLayoutInfo(mWindowToken);
    }

    private void report(String source, SidecarDeviceState deviceState,
            SidecarWindowLayoutInfo layoutInfo) {
        runOnUiThread(() -> reportText(format(source, deviceState, layoutInfo)));
    }

    private void reportText(String message) {
        mOutput.setText(message);
        Log.i(TAG, message);
    }

    private static String format(String source, SidecarDeviceState deviceState,
            SidecarWindowLayoutInfo layoutInfo) {
        StringBuilder output = new StringBuilder(source)
                .append('\n').append("sidecarVersion=").append(SidecarProvider.getApiVersion())
                .append('\n').append("posture=")
                .append(deviceState == null ? "null" : postureName(deviceState.posture));
        List<SidecarDisplayFeature> features = layoutInfo == null || layoutInfo.displayFeatures == null
                ? Collections.emptyList() : layoutInfo.displayFeatures;
        output.append('\n').append("features=").append(features.size());
        for (int index = 0; index < features.size(); index++) {
            SidecarDisplayFeature feature = features.get(index);
            Rect bounds = feature.getRect();
            output.append('\n').append("feature[").append(index + 1).append('/')
                    .append(features.size()).append("] type=")
                    .append(feature.getType() == SidecarDisplayFeature.TYPE_HINGE ? "HINGE" : "FOLD")
                    .append(" bounds=").append(bounds);
        }
        return output.toString();
    }

    private static String postureName(int posture) {
        switch (posture) {
            case SidecarDeviceState.POSTURE_CLOSED:
                return "CLOSED";
            case SidecarDeviceState.POSTURE_HALF_OPENED:
                return "HALF_OPENED";
            case SidecarDeviceState.POSTURE_OPENED:
                return "OPENED";
            case SidecarDeviceState.POSTURE_FLIPPED:
                return "FLIPPED";
            case SidecarDeviceState.POSTURE_UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }
}
