package com.barracoder.android;

import android.app.Activity;
import android.content.SharedPreferences;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

public final class ROMList {
    private static final String TAG = "ROMList";
    private static List<NESItemModel> gameList;
    private static List<NESItemModel> nsfList;
    private static List<NESItemModel> favList;
    private static List<NESItemModel> recentList;

    public static boolean hasChanged = false;

    private static List<NESItemModel> getGameList() {
        if (gameList == null) {
            gameList = new ArrayList<>();
        }
        return gameList;
    }

    private static List<NESItemModel> getNSFList() {
        if (nsfList == null) {
            nsfList = new ArrayList<>();
        }
        return nsfList;
    }

    private static List<NESItemModel> getFavList() {
        if (favList == null) {
            favList = new ArrayList<>();
        }
        return favList;
    }

    private static List<NESItemModel> getRecentList() {
        if (recentList == null) {
            recentList = new ArrayList<>();
        }
        return recentList;
    }

    private static String getBaseName(String fileName) {
        if (fileName.indexOf(".") > 0) {
            return fileName.substring(0, fileName.lastIndexOf("."));
        }
        return fileName;
    }

    private static String[] getROMImages(Activity activity) {
        try {
            return activity.getAssets().list("images");
        } catch (IOException e) {
            Log.d(TAG, "Could not access images folder");
            return new String[0];
        }
    }

    public static void addToCategory(Activity activity, NESItemModel item, String category) {
        if (item == null || item.getRom() == null || category == null) return;
        SharedPreferences pref = activity.getSharedPreferences("com.barracoder.android.tv", Activity.MODE_PRIVATE);
        String roms = pref.getString(category, "");

        if( category.equals("favorites") && !roms.contains(item.getRom())) {
            roms = String.valueOf(TextUtils.concat(roms, ";", item.getRom()));
            favList.add(item);
        } else if (category.equals("recent")) {
            recentList.remove(item);
            recentList.add(0, item);
            // limit recent list to 20 items
            if (recentList.size() > 20) {
                recentList.remove(recentList.size() - 1);
            }
            roms = "";
            for (NESItemModel recentItem : recentList) {
                roms = String.valueOf(TextUtils.concat(roms, ";", recentItem.getRom()));
            }
            roms = roms.startsWith(";") ? roms.substring(1) : roms; // remove leading semicolon
        }
        pref.edit().putString(category, roms).apply();
        hasChanged = true;
    }

    public static void removeFromCategory(Activity activity, NESItemModel item, String category) {
        if (item == null || item.getRom() == null || category == null) return;
        SharedPreferences pref = activity.getSharedPreferences("com.barracoder.android.tv", Activity.MODE_PRIVATE);
        List<String> roms = new LinkedList<>(Arrays.asList(pref.getString(category, "").split(";")));
        if (category.equals("favorites")) {
            roms.remove(item.getRom());
            favList.remove(item);
        } else if (category.equals("recent")) {
            roms.remove(item.getRom());
            recentList.remove(item);
        }
        pref.edit().putString(category, TextUtils.join(";", roms)).apply();
        hasChanged = true;
    }

    public static boolean isInCategory(Activity activity, NESItemModel item, String category) {
        if (item == null || item.getRom() == null || category == null) return false;
        SharedPreferences pref = activity.getSharedPreferences("com.barracoder.android.tv", Activity.MODE_PRIVATE);
        return pref.getString(category, "").contains(item.getRom());
    }

    public static List<NESItemModel> searchByName(String name, String category) {
        List<NESItemModel> results = new ArrayList<>(), targetList;

        if(category == null || category.isEmpty()) return results;

        if (name == null || name.isEmpty()) return results;

        if(category.equals("All Games")) {
            targetList = getGameList();
        } else if(category.equals("Music")) {
            targetList = getNSFList();
        } else {
            Log.e(TAG, "Unsupported search category: " + category);
            return results;
        }

        for (NESItemModel item : targetList) {
            if (item.getRom().toLowerCase(Locale.US).contains(name.toLowerCase(Locale.US))) {
                results.add(item);
            }
        }
        return results;
    }


    public static HashMap<String, List<NESItemModel>> getCategoryMap(Activity activity) {
        HashMap<String, List<NESItemModel>> categoryMap = new LinkedHashMap<>();
        setupROMS(activity);
        categoryMap.put("All Games", getGameList());
        categoryMap.put("Recent", getRecentList());
        categoryMap.put("Favorites", getFavList());
        categoryMap.put("Music", getNSFList());
        return categoryMap;
    }

    public static HashMap<String, List<NESItemModel>> getVolatileCategoryMap(Activity activity) {
        HashMap<String, List<NESItemModel>> categoryMap = new LinkedHashMap<>();
        setupROMS(activity);
        categoryMap.put("Favorites", getFavList());
        categoryMap.put("Recent", getRecentList());
        return categoryMap;
    }

    private static String loadJSONFromAssets(Activity activity, String fileName) {
        try {
            InputStream is = activity.getAssets().open(fileName);
            int size = is.available();
            byte[] buffer = new byte[size];
            is.read(buffer);
            is.close();
            return new String(buffer, StandardCharsets.UTF_8);
        } catch (IOException e) {
            Log.e(TAG, "Could not load JSON from assets: " + fileName, e);
            return null;
        }
    }

    private static HashMap<String, HashMap<String, String>> loadGamesJSON(Activity activity) {
        String json = loadJSONFromAssets(activity, "games.json");
        if (json == null || json.isEmpty()) {
            Log.e(TAG, "games.json is empty or not found");
            return new HashMap<>();
        }
        try {
            JSONArray jsonArray = new JSONArray(json);
            HashMap<String, HashMap<String, String>> gameMap = new HashMap<>();
            for (int i = 0; i < jsonArray.length(); i++) {
                JSONObject item = jsonArray.getJSONObject(i);

                HashMap<String, String> rom = new HashMap<>();
                rom.put("Game", item.optString("Game", ""));
                rom.put("Year", String.valueOf(item.optInt("Year", 0)));
                rom.put("Dev", item.optString("Dev", ""));
                rom.put("Publisher", item.optString("Publisher", ""));

                gameMap.put(Objects.requireNonNull(rom.get("Game")).toLowerCase(Locale.ROOT), rom);

            }
            return gameMap;

        } catch (JSONException e) {
            Log.e("NesRomJsonLoader", "JSON parsing error: " + e.getMessage(), e);
            return new HashMap<>();
        }
    }
    private static void setupROMS(Activity activity) {
        if(gameList != null && nsfList != null && favList != null && recentList != null) {
            Log.d(TAG, "ROMs already set up");
            return;
        }
        gameList = new ArrayList<>();
        nsfList = new ArrayList<>();
        favList = new ArrayList<>();
        recentList = new ArrayList<>();
        try {
            String[] roms = activity.getAssets().list("roms");
            if (roms == null || roms.length == 0) {
                Log.e(TAG, "No ROMS found");
                return;
            }
            SharedPreferences pref = activity.getSharedPreferences("com.barracoder.android.tv", Activity.MODE_PRIVATE);
            String favorites = pref.getString("favorites", "");
            String recent = pref.getString("recent", "");

            // load games.json in assets
            HashMap<String, HashMap<String, String>> gameMap = loadGamesJSON(activity);

            ArrayList<String> images = new ArrayList<>(Arrays.asList(getROMImages(activity)));
            for (String rom : roms) {
                String imageFile = String.format(Locale.US, "%s.jpg", getBaseName(rom));
                Uri imageUri = null;
                if (images.contains(imageFile)) {
                    imageUri = Uri.parse("file:///android_asset/images/" + imageFile);
                }
                NESItemModel item = new NESItemModel(imageUri, getBaseName(rom), "roms/" + rom);
                HashMap<String, String> gameInfo = gameMap.get(item.getName().toLowerCase(Locale.ROOT));
                if(gameInfo != null) {
                    item.setYear(Integer.parseInt(Objects.requireNonNull(gameInfo.get("Year"))));
                    item.setDeveloper(gameInfo.get("Dev"));
                    item.setPublisher(gameInfo.get("Publisher"));
                }
                if (item.isNSF()) {
                    nsfList.add(item);
                } else if (item.isNES()) {
                    gameList.add(item);
                }

                if(favorites.contains(item.getRom())) {
                    favList.add(item);
                }
                if(recent.contains(item.getRom())) {
                    recentList.add(item);
                }
            }
            // Sort lists based on location in favorites and recent strings
            Collections.sort(favList, (a, b) -> {
                int indexA = favorites.indexOf(a.getRom());
                int indexB = favorites.indexOf(b.getRom());
                return Integer.compare(indexA, indexB);
            });
            Collections.sort(recentList, (a, b) -> {
                int indexA = recent.indexOf(a.getRom());
                int indexB = recent.indexOf(b.getRom());
                return Integer.compare(indexA, indexB);
            });
        } catch (IOException e) {
            Log.e(TAG, "Could not access ROMS folder");
        }
    }
}
