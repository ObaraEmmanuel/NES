package com.barracoder.android;

import android.content.pm.ActivityInfo;
import android.graphics.Point;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;

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

    @Override
    protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        mEmulatorFile = getIntent().getStringExtra("rom");
        mGenie = getIntent().getBooleanExtra("genie", false);
        Display display = getWindowManager().getDefaultDisplay();
        Point point = new Point();
        display.getSize(point);
        mWidth = point.x;
        mHeight = point.y;
        Log.i("ANDRONES_EMULATOR", "size: " + point);
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint){
        // Force sensorLandscape
        mSingleton.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

}