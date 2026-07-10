package org.lineageos.fujisan.secondarysysui;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.WallpaperManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public final class SecondaryLauncherActivity extends Activity {
    private static final int SECONDARY_DISPLAY_ID = 1;
    private static final int STATUS_BAR_DP = 24;
    private static final int NAV_BAR_DP = 56;

    private PackageManager mPackageManager;
    private final ArrayList<AppEntry> mApps = new ArrayList<AppEntry>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mPackageManager = getPackageManager();
        loadApps();
        setContentView(buildContentView());
    }

    @Override
    protected void onResume() {
        super.onResume();
        startService(new Intent(this, SecondarySystemUiService.class));
    }

    private View buildContentView() {
        FrameLayout root = new FrameLayout(this);

        ImageView wallpaper = new ImageView(this);
        wallpaper.setScaleType(ImageView.ScaleType.CENTER_CROP);
        Drawable drawable = loadWallpaper();
        if (drawable != null) {
            wallpaper.setImageDrawable(drawable);
        } else {
            wallpaper.setImageDrawable(makeFallbackBackground());
        }
        root.addView(wallpaper, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        View scrim = new View(this);
        scrim.setBackgroundColor(0x55000000);
        root.addView(scrim, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        GridView grid = new GridView(this);
        grid.setNumColumns(4);
        grid.setGravity(Gravity.CENTER);
        grid.setClipToPadding(false);
        grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);
        grid.setVerticalSpacing(dp(14));
        grid.setHorizontalSpacing(dp(6));
        grid.setPadding(dp(12), dp(STATUS_BAR_DP + 24), dp(12), dp(NAV_BAR_DP + 24));
        grid.setBackgroundColor(Color.TRANSPARENT);
        grid.setSelector(android.R.color.transparent);
        grid.setAdapter(new AppsAdapter());
        root.addView(grid, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        return root;
    }

    private Drawable loadWallpaper() {
        try {
            return WallpaperManager.getInstance(this).getDrawable();
        } catch (RuntimeException e) {
            return null;
        }
    }

    private Drawable makeFallbackBackground() {
        GradientDrawable drawable = new GradientDrawable(
                GradientDrawable.Orientation.TL_BR,
                new int[] { 0xff24476b, 0xff172430, 0xff101820 });
        return drawable;
    }

    private void loadApps() {
        mApps.clear();
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        List<ResolveInfo> infos = mPackageManager.queryIntentActivities(intent, 0);
        for (ResolveInfo info : infos) {
            if (info.activityInfo == null) {
                continue;
            }
            if (getPackageName().equals(info.activityInfo.packageName)) {
                continue;
            }
            mApps.add(new AppEntry(info));
        }
        Collections.sort(mApps, new Comparator<AppEntry>() {
            @Override
            public int compare(AppEntry left, AppEntry right) {
                return left.label.compareToIgnoreCase(right.label);
            }
        });
    }

    private void launch(AppEntry entry) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(entry.componentName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(SECONDARY_DISPLAY_ID);
        try {
            startActivity(intent, options.toBundle());
        } catch (RuntimeException e) {
            Toast.makeText(this, entry.label, Toast.LENGTH_SHORT).show();
        }
    }

    private TextView makeLabel(String text, int sp) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(Color.WHITE);
        view.setTextSize(sp);
        view.setGravity(Gravity.CENTER);
        view.setIncludeFontPadding(false);
        view.setMaxLines(2);
        return view;
    }

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
    }

    private final class AppsAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return mApps.size();
        }

        @Override
        public Object getItem(int position) {
            return mApps.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final AppEntry entry = mApps.get(position);
            LinearLayout item;
            ImageView icon;
            TextView label;
            if (convertView instanceof LinearLayout) {
                item = (LinearLayout) convertView;
                icon = (ImageView) item.getChildAt(0);
                label = (TextView) item.getChildAt(1);
            } else {
                item = new LinearLayout(SecondaryLauncherActivity.this);
                item.setOrientation(LinearLayout.VERTICAL);
                item.setGravity(Gravity.CENTER);
                item.setPadding(dp(4), dp(8), dp(4), dp(8));
                icon = new ImageView(SecondaryLauncherActivity.this);
                item.addView(icon, new LinearLayout.LayoutParams(dp(56), dp(56)));
                label = makeLabel("", 12);
                LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT, dp(40));
                labelParams.topMargin = dp(7);
                item.addView(label, labelParams);
            }

            icon.setImageDrawable(entry.icon);
            label.setText(entry.label);
            item.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    launch(entry);
                }
            });
            return item;
        }
    }

    private final class AppEntry {
        final String label;
        final Drawable icon;
        final ComponentName componentName;

        AppEntry(ResolveInfo info) {
            label = info.loadLabel(mPackageManager).toString();
            icon = info.loadIcon(mPackageManager);
            componentName = new ComponentName(info.activityInfo.packageName, info.activityInfo.name);
        }
    }
}
