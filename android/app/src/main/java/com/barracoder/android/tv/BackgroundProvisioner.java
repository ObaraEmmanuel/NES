package com.barracoder.android.tv;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.leanback.app.BackgroundManager;

import com.barracoder.android.R;
import com.bumptech.glide.Glide;
import com.bumptech.glide.request.target.CustomTarget;
import com.bumptech.glide.request.transition.Transition;

import java.io.IOException;
import java.util.Objects;
import java.util.Timer;
import java.util.TimerTask;

public final class BackgroundProvisioner {
    private static final String TAG = "DefaultBgMgr";
    private static final int BACKGROUND_UPDATE_DELAY = 20000;
    private static BackgroundProvisioner INSTANCE;
    private String mBackgroundUri;
    private Timer mBackgroundTimer;
    private Drawable mDefaultBackground;
    private String[] mBackgrounds;

    private DisplayMetrics mMetrics;

    private final Handler mHandler = new Handler(Objects.requireNonNull(Looper.myLooper()));

    public interface BackgroundSetter {

        default AssetManager getAssetsForBackground() {
            return getActivityForBackground().getAssets();
        }

        Activity getActivityForBackground();

        default DisplayMetrics getDisplayMetrics(){
            DisplayMetrics metrics = new DisplayMetrics();
            getActivityForBackground().getWindowManager().getDefaultDisplay().getMetrics(metrics);
            return metrics;
        }

        default BackgroundManager getBackgroundManager() {
            return BackgroundManager.getInstance(getActivityForBackground());
        }
    }

    private BackgroundSetter mBackgroundSetter;

    private BackgroundProvisioner() {
        if (INSTANCE == null) {
            INSTANCE = this;
        }
    }

    private void loadBackgrounds() {
        if (mBackgrounds == null) {
            try {
                mBackgrounds = mBackgroundSetter.getAssetsForBackground().list("backgrounds");
            } catch (IOException e) {
                Log.e(TAG, "Error loading backgrounds", e);
                mBackgrounds = new String[0];
            }
            mDefaultBackground = ContextCompat.getDrawable(
                    mBackgroundSetter.getActivityForBackground(), R.drawable.default_background
            );
        }
    }

    private void startBackgroundTimer() {
        if (null != mBackgroundTimer) {
            mBackgroundTimer.cancel();
        }
        mBackgroundTimer = new Timer();
        mBackgroundTimer.schedule(new UpdateBackgroundTask(), BACKGROUND_UPDATE_DELAY);
    }

    private String selectRandomBackground() {
        if (mBackgrounds.length > 0) {
            int randomIndex = (int) (Math.random() * mBackgrounds.length);
            return  "file:///android_asset/backgrounds/" + mBackgrounds[randomIndex];
        } else {
            return "file:///android_asset/backgrounds/contra.jpg";
        }
    }

    private void updateBackground(String uri) {
        if (mBackgroundSetter == null) {
            return;
        }

        int width = mMetrics.widthPixels;
        int height = mMetrics.heightPixels;
        Glide.with(mBackgroundSetter.getActivityForBackground())
                .load(uri)
                .centerCrop()
                .error(mDefaultBackground)
                .into(new CustomTarget<Drawable>(width, height) {
                    @Override
                    public void onResourceReady(@NonNull Drawable drawable,
                                                @Nullable Transition<? super Drawable> transition) {
                        mBackgroundSetter.getBackgroundManager().setDrawable(drawable);
                    }

                    @Override
                    public void onLoadCleared(@Nullable Drawable placeholder) {

                    }
                });
        if (mBackgroundTimer != null)
            mBackgroundTimer.cancel();
        mBackgroundUri = uri;
    }


    public void setBackgroundSetter(BackgroundSetter backgroundSetter) {
        if( backgroundSetter == mBackgroundSetter) {
            return;
        }
        mBackgroundSetter = backgroundSetter;
        loadBackgrounds();
        try {
            mBackgroundSetter.getBackgroundManager().attach(
                    mBackgroundSetter.getActivityForBackground().getWindow()
            );
        } catch (RuntimeException e){
            // expected if the activity has already been attached
        }
        mMetrics = mBackgroundSetter.getDisplayMetrics();

        if(mBackgroundUri == null){
            mBackgroundUri = selectRandomBackground();
        }
        updateBackground(mBackgroundUri);
        startBackgroundTimer();
    }

    public static BackgroundProvisioner forActivity(BackgroundSetter setter) {
        if (INSTANCE == null) {
            INSTANCE = new BackgroundProvisioner();
        }
        INSTANCE.setBackgroundSetter(setter);
        return INSTANCE;
    }

    public void forgetActivity(BackgroundSetter setter) {
        if(setter != mBackgroundSetter){
            return;
        }
        mBackgroundSetter = null;
        if(mBackgroundTimer != null) {
            mBackgroundTimer.cancel();
            mBackgroundTimer = null;
        }
    }

    private class UpdateBackgroundTask extends TimerTask {
        @Override
        public void run() {
            mHandler.post(() -> {
                updateBackground(selectRandomBackground());
                startBackgroundTimer();
            });
        }
    }
}
