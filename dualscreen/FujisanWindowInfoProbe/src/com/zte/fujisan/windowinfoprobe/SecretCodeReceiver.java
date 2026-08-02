package com.zte.fujisan.windowinfoprobe;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/** Opens the foreground-only probe from the dialer code *#*#38986#*#*. */
public final class SecretCodeReceiver extends BroadcastReceiver {
    private static final String TAG = "FujisanWindowProbe";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i(TAG, "Secret code received: " + intent.getData());
        Intent launchIntent = new Intent(context, MainActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        context.startActivity(launchIntent);
    }
}
