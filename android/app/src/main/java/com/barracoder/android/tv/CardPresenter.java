package com.barracoder.android.tv;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.leanback.widget.ImageCardView;
import androidx.leanback.widget.Presenter;

import com.barracoder.android.NESItemModel;
import com.barracoder.android.R;
import com.bumptech.glide.Glide;

import java.util.Objects;

public class CardPresenter extends Presenter {
    private static final String TAG = "CardPresenter";
    private static int sSelectedBackgroundColor;
    private static int sDefaultBackgroundColor;
    private Drawable mDefaultCardImage;
    private DisplayMetrics mMetrics;
    private static void updateCardBackgroundColor(ImageCardView view, boolean selected) {
        int color = selected ? sSelectedBackgroundColor : sDefaultBackgroundColor;
        // Both background colors should be set because the view"s background is temporarily visible
        // during animations.
        view.setBackgroundColor(color);
        view.setInfoAreaBackgroundColor(color);
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent) {
        Log.d(TAG, "onCreateViewHolder");

        sDefaultBackgroundColor =
                ContextCompat.getColor(parent.getContext(), R.color.default_background);
        sSelectedBackgroundColor =
                ContextCompat.getColor(parent.getContext(), R.color.selected_background);
        /*
         * This template uses a default image in res/drawable, but the general case for Android TV
         * will require your resources in xhdpi. For more information, see
         * https://developer.android.com/training/tv/start/layouts.html#density-resources
         */
        mDefaultCardImage = ContextCompat.getDrawable(parent.getContext(), R.drawable.controller_default);

        ImageCardView cardView =
                new ImageCardView(parent.getContext()) {
                    @Override
                    public void setSelected(boolean selected) {
                        updateCardBackgroundColor(this, selected);
                        super.setSelected(selected);
                    }
                };

        cardView.setFocusable(true);
        cardView.setFocusableInTouchMode(true);
        cardView.setLongClickable(true);
        cardView.setAlpha(0.8f);
        assert cardView.getMainImageView() != null;
        cardView.getMainImageView().setBackground(
                ContextCompat.getDrawable(parent.getContext(), R.drawable.box_gradient)
        );
        updateCardBackgroundColor(cardView, false);
        return new ViewHolder(cardView);
    }

    private Activity getActivity(View view) {
        Context context = view.getContext();
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity)context;
            }
            context = ((ContextWrapper)context).getBaseContext();
        }
        return null;
    }

    @Override
    public void onBindViewHolder(Presenter.ViewHolder viewHolder, Object item) {
        Log.d(TAG, "onBindViewHolder");
        NESItemModel rom = (NESItemModel) item;
        ImageCardView cardView = (ImageCardView) viewHolder.view;

        if (mMetrics == null) {
            mMetrics = new DisplayMetrics();
            Objects.requireNonNull(getActivity(viewHolder.view)).getWindowManager().getDefaultDisplay().getMetrics(mMetrics);
        }

        cardView.setTitleText(rom.getName());
        cardView.setContentText(rom.getPublisher());
        cardView.setMainImageDimensions(
                (int) (mMetrics.widthPixels * 0.15f),
                (int) (mMetrics.heightPixels * 0.4f)
        );
        assert cardView.getMainImageView() != null;
        if (rom.getImage() != null) {
            Glide.with(viewHolder.view.getContext())
                    .load(rom.getImage())
                    .error(mDefaultCardImage)
                    .into(cardView.getMainImageView());
        } else if (rom.isNSF()) {
            cardView.getMainImageView().setImageResource(R.drawable.music_default);
        } else {
            cardView.getMainImageView().setImageResource(R.drawable.controller_default);
        }
    }

    @Override
    public void onUnbindViewHolder(Presenter.ViewHolder viewHolder) {
        Log.d(TAG, "onUnbindViewHolder");
        ImageCardView cardView = (ImageCardView) viewHolder.view;
        // Remove references to images so that the garbage collector can free up memory
        cardView.setBadgeImage(null);
        cardView.setMainImage(null);
    }
}