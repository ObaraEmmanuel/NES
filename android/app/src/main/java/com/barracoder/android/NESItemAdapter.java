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
        NESItemHolder holder = new NESItemHolder(view);

        holder.playButton.setOnClickListener(v -> {
            if(holder.item == null)
                return;
            Intent intent = new Intent(context, EmulatorActivity.class);
            intent.putExtra("rom", holder.item.getRom());
            context.startActivity(intent);
        });

        holder.magicButton.setOnClickListener(v -> {
            if(holder.item == null || !holder.item.isNES())
                return;
            Intent intent = new Intent(context, EmulatorActivity.class);
            intent.putExtra("rom", holder.item.getRom());
            intent.putExtra("genie", true);
            context.startActivity(intent);
        });
        return holder;
    }

    @Override
    public void onBindViewHolder(@NonNull NESItemHolder holder, int position) {
        NESItemModel item = list.get(position);
        holder.item = item;
        holder.gameName.setText(item.getName());
        if(item.getImage() != null)
            Glide.with(context).load(item.getImage()).into(holder.gameImage);
        else if (item.isNSF())
            holder.gameImage.setImageResource(R.drawable.music);
        else
            holder.gameImage.setImageResource(R.drawable.controller);

        holder.playButton.setText(item.isNES()? context.getString(R.string.play) : context.getString(R.string.listen));

        MainActivity activity = (MainActivity) context;
        if(activity.hasGenie && item.isNES())
            holder.magicButton.setVisibility(View.VISIBLE);
        else
            holder.magicButton.setVisibility(View.GONE);
    }

    @Override
    public int getItemCount() {
        return list.size();
    }
}
