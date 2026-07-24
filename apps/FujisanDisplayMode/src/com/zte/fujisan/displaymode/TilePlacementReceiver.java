package com.zte.fujisan.displaymode;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Seeds the custom tile once, without overwriting a user's later QS layout. */
public final class TilePlacementReceiver extends BroadcastReceiver {
    private static final String QS_TILES = "sysui_qs_tiles";
    private static final String SEEDED = "fujisan_display_mode_tile_seeded";
    private static final String TILE_SPEC =
            "custom(com.zte.fujisan.displaymode/com.zte.fujisan.displaymode.FujisanDisplayModeTile)";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Settings.Secure.getInt(context.getContentResolver(), SEEDED, 0) != 0) return;

        final String existing = Settings.Secure.getString(context.getContentResolver(), QS_TILES);
        if (existing == null || existing.isEmpty()) return; // The RRO supplies first-boot defaults.

        final List<String> specs = new ArrayList<>(Arrays.asList(existing.split(",")));
        if (!specs.contains(TILE_SPEC)) {
            // Slot 6 (after the stock Internet, Bluetooth, Flashlight, DND and Alarm tiles).
            specs.add(Math.min(5, specs.size()), TILE_SPEC);
            Settings.Secure.putString(context.getContentResolver(), QS_TILES,
                    String.join(",", specs));
        }
        Settings.Secure.putInt(context.getContentResolver(), SEEDED, 1);
    }
}
