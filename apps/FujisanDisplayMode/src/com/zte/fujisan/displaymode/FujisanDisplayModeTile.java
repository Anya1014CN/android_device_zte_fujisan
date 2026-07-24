package com.zte.fujisan.displaymode;

import android.os.SystemProperties;
import android.graphics.drawable.Icon;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/** Quick Settings switch for Fujisan's two display topologies. */
public final class FujisanDisplayModeTile extends TileService {
    private static final String USER_MODE_PROPERTY = "persist.vendor.fujisan.user_mode";

    private boolean isDualMode() {
        final String mode = SystemProperties.get(USER_MODE_PROPERTY, "");
        if ("dual".equals(mode)) return true;
        if ("zoom".equals(mode)) return false;
        return SystemProperties.getBoolean("persist.vendor.fujisan.dual_internal", false);
    }

    private void refreshTile() {
        final Tile tile = getQsTile();
        if (tile == null) return;

        final boolean dual = isDualMode();
        tile.setState(dual ? Tile.STATE_INACTIVE : Tile.STATE_ACTIVE);
        tile.setLabel(getString(dual ? R.string.tile_dual : R.string.tile_extension));
        tile.setSubtitle(getString(dual ? R.string.tile_dual_subtitle
                : R.string.tile_extension_subtitle));
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
        SystemProperties.set(USER_MODE_PROPERTY, isDualMode() ? "zoom" : "dual");
        refreshTile();
    }
}
