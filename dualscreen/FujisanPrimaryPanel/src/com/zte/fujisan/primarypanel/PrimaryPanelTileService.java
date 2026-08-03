package com.zte.fujisan.primarypanel;

import android.graphics.drawable.Icon;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.SystemProperties;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.util.Log;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class PrimaryPanelTileService extends TileService {
    private static final String TAG = "FujisanPrimaryPanel";
    private static final String SOCKET_NAME = "fujisan_primary";
    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();
    private boolean mPropertyCallbackRegistered;

    @Override
    public void onCreate() {
        super.onCreate();
        if (!mPropertyCallbackRegistered) {
            SystemProperties.addChangeCallback(() -> mExecutor.execute(this::refreshTile));
            mPropertyCallbackRegistered = true;
        }
    }

    @Override
    public void onStartListening() {
        super.onStartListening();
        mExecutor.execute(this::refreshTile);
    }

    @Override
    public void onClick() {
        mExecutor.execute(() -> {
            final String response = request("toggle\n");
            if (!response.startsWith("ok ") && !response.startsWith("unavailable ")) {
                Log.w(TAG, "primary-panel request failed: " + response);
            }
            refreshTile();
        });
    }

    private void refreshTile() {
        final Tile tile = getQsTile();
        if (tile == null) {
            return;
        }
        final boolean closed = "1".equals(SystemProperties.get("vendor.fujisan.hall_status", "0"));
        final boolean panelB = "b".equals(SystemProperties.get(
                "persist.vendor.fujisan.primary_panel", "a"));
        tile.setLabel(getString(R.string.tile_title));
        tile.setSubtitle(getString(closed
                ? (panelB ? R.string.panel_b : R.string.panel_a)
                : R.string.fold_device));
        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_primary_panel));
        /* This is an action selector, not a Boolean feature.  Keep its
         * visible state off for both A and B; unavailable still prevents a
         * stale click while the hinge is open. */
        tile.setState(closed ? Tile.STATE_INACTIVE : Tile.STATE_UNAVAILABLE);
        tile.updateTile();
    }

    private String request(String command) {
        try (LocalSocket socket = new LocalSocket()) {
            socket.connect(new LocalSocketAddress(SOCKET_NAME, LocalSocketAddress.Namespace.RESERVED));
            socket.getOutputStream().write(command.getBytes(StandardCharsets.US_ASCII));
            socket.getOutputStream().flush();
            final byte[] response = new byte[32];
            final int read = socket.getInputStream().read(response);
            return read > 0 ? new String(response, 0, read, StandardCharsets.US_ASCII).trim() : "error empty";
        } catch (IOException e) {
            Log.e(TAG, "primary-panel socket", e);
            return "error socket";
        }
    }
}
