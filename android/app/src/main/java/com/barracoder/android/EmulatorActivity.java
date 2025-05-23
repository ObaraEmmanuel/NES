package com.barracoder.android;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Pair;
import android.view.WindowMetrics;

import org.libsdl.app.SDLActivity;

import java.util.Arrays;

public class EmulatorActivity extends SDLActivity {
    private String mEmulatorFile;
    private boolean mGenie;
    private int mWidth, mHeight;

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL3",
                "SDL3_ttf",
                "nes"
        };
    }

    @Override
    protected String[] getArguments() {
        String[] args = new String[]{
                mEmulatorFile,
                String.valueOf(mWidth),
                String.valueOf(mHeight)
        };
        if(mGenie) {
            args = Arrays.copyOf(args, args.length + 1);
            args[args.length - 1] = "roms/GENIE.nes";
        }
        return args;
    }

    public static Pair<Integer, Integer> getScreenDimensions(Activity activity) {
        int width;
        int height;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowMetrics metrics = activity.getWindowManager().getCurrentWindowMetrics();
            Rect bounds = metrics.getBounds();
            width = bounds.width();
            height = bounds.height();
        } else {
            DisplayMetrics metrics = new DisplayMetrics();
            activity.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            width = metrics.widthPixels;
            height = metrics.heightPixels;
        }

        return new Pair<>(width, height);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        mEmulatorFile = getIntent().getStringExtra("rom");
        mGenie = getIntent().getBooleanExtra("genie", false);

        Pair<Integer, Integer> dimensions = getScreenDimensions(this);
        mWidth = dimensions.first;
        mHeight = dimensions.second;
        Log.i("ANDRONES_EMULATOR", "size: " + mWidth + "x" + mHeight);
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint){
        // Force sensorLandscape
        mSingleton.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

}