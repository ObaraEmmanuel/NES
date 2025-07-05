package com.barracoder.android;

import android.annotation.SuppressLint;
import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.BatteryManager;
import android.os.Build;

public class UiModeHelper {

    public enum UiModeType {
        NORMAL,
        DESK,
        CAR,
        TV,
        APPLIANCE,
        WATCH,
        VR
    }

    public static UiModeType getUiModeType(Context context) {
        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        PackageManager packageManager = context.getPackageManager();
        BatteryManager batteryManager = (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);

        int modeType = uiModeManager != null ? uiModeManager.getCurrentModeType() : Configuration.UI_MODE_TYPE_NORMAL;

        if (modeType == Configuration.UI_MODE_TYPE_APPLIANCE) {
            return UiModeType.APPLIANCE;
        } else if (modeType == Configuration.UI_MODE_TYPE_CAR) {
            return UiModeType.CAR;
        } else if (modeType == Configuration.UI_MODE_TYPE_DESK) {
            return UiModeType.DESK;
        } else if (modeType == Configuration.UI_MODE_TYPE_TELEVISION) {
            return UiModeType.TV;
        } else if (modeType == Configuration.UI_MODE_TYPE_WATCH) {
            return UiModeType.WATCH;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O &&
                modeType == Configuration.UI_MODE_TYPE_VR_HEADSET) {
            return UiModeType.VR;
        } else if (isLikelyTelevision(context, packageManager, batteryManager)) {
            return UiModeType.TV;
        } else if (modeType == Configuration.UI_MODE_TYPE_NORMAL) {
            return UiModeType.NORMAL;
        } else {
            return UiModeType.NORMAL;
        }
    }

    private static boolean isLikelyTelevision(Context context, PackageManager pm, BatteryManager bm) {
        if (pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK)) {
            return true;
        }

        return isBatteryAbsent(bm) &&
                pm.hasSystemFeature(PackageManager.FEATURE_USB_HOST) &&
                (Build.VERSION.SDK_INT < Build.VERSION_CODES.N || pm.hasSystemFeature(PackageManager.FEATURE_ETHERNET)) &&
                !pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN);
    }

    @SuppressLint("NewApi")
    private static boolean isBatteryAbsent(BatteryManager bm) {
        if (bm != null) {
            return bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY) == 0;
        }
        return false;
    }
}
