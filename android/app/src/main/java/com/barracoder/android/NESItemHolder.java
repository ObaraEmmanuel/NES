package com.barracoder.android;

import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

public class NESItemHolder  extends RecyclerView.ViewHolder {
    ImageView gameImage;
    TextView gameName;
    Button playButton;

    public NESItemHolder(View itemView){
        super(itemView);
        gameImage = itemView.findViewById(R.id.gameImage);
        gameName = itemView.findViewById(R.id.gameName);
        playButton = itemView.findViewById(R.id.playButton);
    }
}
