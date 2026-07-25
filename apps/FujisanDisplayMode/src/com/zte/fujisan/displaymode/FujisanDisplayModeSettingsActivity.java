package com.zte.fujisan.displaymode;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemProperties;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

/** The implementation behind Settings > Display > Dual-screen mode. */
public final class FujisanDisplayModeSettingsActivity extends Activity {
    private static final String USER_MODE_PROPERTY = "persist.vendor.fujisan.user_mode";
    private static final String HALL_STATUS_PROPERTY = "vendor.fujisan.hall_status";
    private static final String LEGACY_DUAL_PROPERTY = "persist.vendor.fujisan.dual_internal";

    private RadioGroup mModes;
    private TextView mDescription;
    private boolean mUpdating;

    private static int dp(Activity activity, int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }

    private boolean isOpen() {
        return SystemProperties.getInt(HALL_STATUS_PROPERTY, 1) != 1;
    }

    private boolean isDualMode() {
        final String mode = SystemProperties.get(USER_MODE_PROPERTY, "");
        return "dual".equals(mode) || (mode.isEmpty()
                && SystemProperties.getBoolean(LEGACY_DUAL_PROPERTY, false));
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        // Settings uses a static resource overlay to link here.  Keep the
        // topology policy in this device app: folded hardware must never
        // expose a mode selector, even when launched from an old Settings
        // process whose preference screen has not been recreated yet.
        if (!isOpen()) {
            Toast.makeText(this, R.string.settings_unfold_required, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        setTitle(R.string.settings_display_mode_title);

        final LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.START);
        final int padding = dp(this, 24);
        root.setPadding(padding, padding, padding, padding);

        mDescription = new TextView(this);
        mDescription.setTextSize(16);
        root.addView(mDescription, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        mModes = new RadioGroup(this);
        mModes.setOrientation(RadioGroup.VERTICAL);
        final LinearLayout.LayoutParams groupParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        groupParams.topMargin = dp(this, 16);
        root.addView(mModes, groupParams);

        final RadioButton extended = new RadioButton(this);
        extended.setId(1);
        extended.setText(R.string.settings_extension);
        mModes.addView(extended);

        final RadioButton independent = new RadioButton(this);
        independent.setId(2);
        independent.setText(R.string.settings_dual);
        mModes.addView(independent);

        mModes.setOnCheckedChangeListener((group, checkedId) -> {
            if (mUpdating || !isOpen()) return;
            SystemProperties.set(USER_MODE_PROPERTY, checkedId == 2 ? "dual" : "zoom");
            refresh();
        });
        setContentView(root);
        refresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh() {
        if (mModes == null) return;
        final boolean open = isOpen();
        if (!open) {
            mDescription.setText(R.string.settings_unfold_required);
            mModes.setEnabled(false);
            for (int i = 0; i < mModes.getChildCount(); ++i) {
                mModes.getChildAt(i).setEnabled(false);
            }
            return;
        }
        mDescription.setText(R.string.settings_display_mode_summary);
        mModes.setEnabled(true);
        for (int i = 0; i < mModes.getChildCount(); ++i) {
            mModes.getChildAt(i).setEnabled(true);
        }
        mUpdating = true;
        mModes.check(isDualMode() ? 2 : 1);
        mUpdating = false;
    }
}
