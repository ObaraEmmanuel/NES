package com.barracoder.android;

import android.net.Uri;

import androidx.annotation.NonNull;

import java.util.Locale;

public class NESItemModel {
    private String rom;
    private Uri image;
    private String name;

    public boolean isNSF;
    public boolean isNES;

    public NESItemModel(Uri image, String name, String rom) {
        this.image = image;
        this.name = name;
        setRom(rom);
    }

    public Uri getImage() {
        return image;
    }

    public void setImage(Uri image) {
        this.image = image;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getRom() {
        return rom;
    }

    public void setRom(String rom) {
        this.rom = rom;
        this.isNES = rom.toLowerCase().endsWith(".nes");
        this.isNSF = rom.toLowerCase().endsWith(".nsf") || rom.toLowerCase().endsWith(".nsfe");
    }

    @NonNull
    @Override
    public String toString() {
        return String.format(Locale.US, "NESItem(image=%s, name=%s)", image, name);
    }
}
