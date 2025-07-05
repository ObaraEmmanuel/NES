package com.barracoder.android;

import android.net.Uri;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

import java.util.Locale;

public class NESItemModel implements Parcelable {
    private String rom;
    private Uri image;
    private String name;
    private int year = 1985;
    private String developer = "Unknown";
    private String publisher = "Unknown";

    public NESItemModel(Uri image, String name, String rom) {
        this.image = image;
        this.name = name;
        setRom(rom);
    }

    public NESItemModel(Uri image, String name, String rom, String developer, String publisher, int year) {
        this(image, name, rom);
        this.developer = developer;
        this.publisher = publisher;
        this.year = year;
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

    public String getDeveloper() {
        return developer;
    }
    public void setDeveloper(String developer) {
        this.developer = developer;
    }
    public String getPublisher() {
        return publisher;
    }
    public void setPublisher(String publisher) {
        this.publisher = publisher;
    }
    public int getYear() {
        return year;
    }
    public void setYear(int year) {
        this.year = year;
    }

    public boolean isNSF() {
        return rom.toLowerCase().endsWith(".nsf") || rom.toLowerCase().endsWith(".nsfe");
    }
    public boolean isNES() {
        return rom.toLowerCase().endsWith(".nes");
    }

    public void setRom(String rom) {
        this.rom = rom;
    }

    @NonNull
    @Override
    public String toString() {
        return String.format(Locale.US, "NESItem(image=%s, name=%s)", image, name);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof NESItemModel)) return false;
        NESItemModel that = (NESItemModel) o;
        return rom.equals(that.rom);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel parcel, int i) {
        parcel.writeParcelable(image, i);
        parcel.writeString(name);
        parcel.writeString(rom);
        parcel.writeString(developer);
        parcel.writeString(publisher);
        parcel.writeInt(year);
    }

    public static final Creator<NESItemModel> CREATOR = new Creator<NESItemModel>() {
        @Override
        public NESItemModel createFromParcel(Parcel in) {
            return new NESItemModel(in.readParcelable(Uri.class.getClassLoader()),
                    in.readString(), in.readString(), in.readString(), in.readString(), in.readInt());
        }

        @Override
        public NESItemModel[] newArray(int size) {
            return new NESItemModel[size];
        }
    };
}
