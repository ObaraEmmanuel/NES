package com.barracoder.android;

import android.graphics.Point;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;

import org.libsdl.app.SDLActivity;

public class EmulatorActivity extends SDLActivity {
    private String mEmulatorFile;
    private int mWidth, mHeight;

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "SDL2_ttf",
                "nes"
        };
    }

    @Override
    protected String[] getArguments() {
        return new String[]{
                mEmulatorFile,
                String.valueOf(mWidth),
                String.valueOf(mHeight)
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        mEmulatorFile = getIntent().getStringExtra("rom");
        Display display = getWindowManager().getDefaultDisplay();
        Point point = new Point();
        display.getSize(point);
        mWidth = point.x;
        mHeight = point.y;
        Log.i("ANDRONES_EMULATOR", "size: " + point);
    }

}