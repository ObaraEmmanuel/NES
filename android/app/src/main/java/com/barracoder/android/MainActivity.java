package com.barracoder.android;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.recyclerview.widget.DefaultItemAnimator;
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
    public boolean hasGenie;
    private static final String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        UiModeHelper.UiModeType mode = UiModeHelper.getUiModeType(this);
        if(mode == UiModeHelper.UiModeType.TV) {
            // If the app is running in TV mode, start the TV activity
            Intent intent = new Intent(this, com.barracoder.android.tv.MainActivity.class);
            startActivity(intent);
            finish();
            return;
        }
        setContentView(R.layout.activity_main);
        hasGenie = assetsHasGenie();

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

        recyclerView.setLayoutManager(new GridAutoFitLayoutManager(getApplicationContext(), recyclerView, 180));
        recyclerView.setItemAnimator(new DefaultItemAnimator());

        String[] roms = get_rom_files();
        ArrayList<String> images = new ArrayList<>(Arrays.asList(get_rom_images()));

        for(String rom: roms){
            String imageFile = String.format(Locale.US, "%s.jpg", get_base_name(rom));
            Uri imageUri = null;
            if(images.contains(imageFile)) {
                imageUri = Uri.parse("file:///android_asset/images/" + imageFile);
            }
            NESItemModel item = new NESItemModel(imageUri, get_base_name(rom), "roms/" + rom);
            list.add(item);
        }

        adapter = new NESItemAdapter(MainActivity.this, list);
        recyclerView.setAdapter(adapter);

        findViewById(R.id.openROMBtn).setOnClickListener(view -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, 42);
        });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 42 && resultCode == RESULT_OK) {
            // launch ROM
            Toast.makeText(this, "Not implemented", Toast.LENGTH_SHORT).show();
        }
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

    private boolean assetsHasGenie(){
        // check if roms/GENIE.nes exists
        return Arrays.asList(get_rom_files()).contains("GENIE.nes");
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