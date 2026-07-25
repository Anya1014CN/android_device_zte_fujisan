package com.zte.fujisan.displaymode;

import android.os.SystemProperties;
import android.graphics.drawable.Icon;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/** Quick Settings switch for Fujisan's two display topologies. */
public final class FujisanDisplayModeTile extends TileService {
    private static final String USER_MODE_PROPERTY = "persist.vendor.fujisan.user_mode";
    private static final String HALL_STATUS_PROPERTY = "vendor.fujisan.hall_status";

    private boolean isDualMode() {
        final String mode = SystemProperties.get(USER_MODE_PROPERTY, "");
        if ("dual".equals(mode)) return true;
        if ("zoom".equals(mode)) return false;
        return SystemProperties.getBoolean("persist.vendor.fujisan.dual_internal", false);
    }

    private boolean isOpen() {
        /* Fujisan's hall state 1 is the only folded posture.  States 2 and
         * 3 expose both inside panels and can safely change topology. */
        return SystemProperties.getInt(HALL_STATUS_PROPERTY, 1) != 1;
    }

    private void refreshTile() {
        final Tile tile = getQsTile();
        if (tile == null) return;

        final boolean open = isOpen();
        final boolean dual = isDualMode();
        tile.setState(open ? (dual ? Tile.STATE_INACTIVE : Tile.STATE_ACTIVE)
                : Tile.STATE_UNAVAILABLE);
        tile.setLabel(getString(R.string.tile_title));
        tile.setSubtitle(getString(open
                ? (dual ? R.string.tile_switch_extension : R.string.tile_switch_dual)
                : R.string.tile_unfold_required));
        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_display_mode));
        tile.updateTile();
    }

    @Override
    public void onStartListening() {
        super.onStartListening();
        refreshTile();
    }

    @Override
    public void onClick() {
        if (!isOpen()) {
            refreshTile();
            return;
        }
        SystemProperties.set(USER_MODE_PROPERTY, isDualMode() ? "zoom" : "dual");
        refreshTile();
    }
}
