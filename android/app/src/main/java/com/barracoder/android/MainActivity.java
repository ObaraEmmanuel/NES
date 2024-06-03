package com.barracoder.android;

import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {

    ArrayList<NESItemModel> list;
    RecyclerView recyclerView;
    NESItemAdapter adapter;
    private static final String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SearchView searchView = findViewById(R.id.searchView);

        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                return false;
            }

            @Override
            public boolean onQueryTextChange(String newText) {
                filter(newText);
                return false;
            }
        });

        recyclerView = findViewById(R.id.NESRecyclerView);
        list = new ArrayList<>();

        recyclerView.setLayoutManager(new GridLayoutManager(getApplicationContext(), 2));
        recyclerView.setItemAnimator(new DefaultItemAnimator());

        String[] roms = get_rom_files();
        ArrayList<String> images = new ArrayList<>(Arrays.asList(get_rom_images()));

        for(String rom: roms){
            String imageFile = String.format(Locale.US, "%s.jpg", get_base_name(rom));
            Uri imageUri = null;
            if(images.contains(imageFile)) {
                imageUri = Uri.fromFile(new File("//android_asset/images/" + imageFile));
            }
            NESItemModel item = new NESItemModel(imageUri, get_base_name(rom), "roms/" + rom);
            list.add(item);
        }

        adapter = new NESItemAdapter(MainActivity.this, list);
        recyclerView.setAdapter(adapter);
    }

    protected String[] get_rom_images(){
        try {
            return getAssets().list("images");
        }catch (IOException e){
            Log.d(TAG, "Could not access images folder");
            return new String[0];
        }
    }

    protected String[] get_rom_files(){
        try {
            return getAssets().list("roms");
        }catch (IOException e){
            Log.d(TAG, "Could not access ROMS folder");
            return new String[0];
        }
    }

    protected String get_base_name(String fileName){
        if (fileName.indexOf(".") > 0) {
            return fileName.substring(0, fileName.lastIndexOf("."));
        }
        return fileName;
    }

    private void filter(String text) {
        ArrayList<NESItemModel> filteredList = new ArrayList<>();
        for (NESItemModel item : list) {
            if (item.getName().toLowerCase().contains(text.toLowerCase())) {
                filteredList.add(item);
            }
        }
        adapter.filterList(filteredList);
    }
}