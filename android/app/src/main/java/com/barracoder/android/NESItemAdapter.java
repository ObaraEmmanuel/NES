package com.barracoder.android;

import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;

import java.util.ArrayList;

public class NESItemAdapter extends RecyclerView.Adapter<NESItemHolder> {

    Context context;
    ArrayList<NESItemModel> list;

    public  NESItemAdapter(Context context, ArrayList<NESItemModel> list){
        this.context = context;
        this.list = list;
    }

    public void filterList(ArrayList<NESItemModel> filteredList){
        list = filteredList;
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public NESItemHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(context).inflate(R.layout.game_item, parent, false);
        return new NESItemHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull NESItemHolder holder, int position) {
        NESItemModel item = list.get(position);
        holder.gameName.setText(item.getName());
        if(item.getImage() != null)
            Glide.with(context).load(item.getImage()).into(holder.gameImage);
        else if (item.getRom().endsWith(".nsf") || item.getRom().endsWith(".nsfe")) {
            holder.gameImage.setImageResource(R.drawable.music);
        } else
            holder.gameImage.setImageResource(R.drawable.controller);

        holder.playButton.setOnClickListener(view -> {
            Intent intent = new Intent(context, EmulatorActivity.class);
            intent.putExtra("rom", item.getRom());
            context.startActivity(intent);
        });
    }

    @Override
    public int getItemCount() {
        return list.size();
    }
}
