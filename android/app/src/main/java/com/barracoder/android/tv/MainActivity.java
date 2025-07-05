package com.barracoder.android.tv;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import com.barracoder.android.R;
import com.barracoder.android.UiModeHelper;

/**
 * Main Activity class that loads {@link MainFragment}.
 */
public class MainActivity extends FragmentActivity {
    private static final String TAG = "MainActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate: Starting MainActivity");
        super.onCreate(savedInstanceState);
        UiModeHelper.UiModeType mode = UiModeHelper.getUiModeType(this);
        if (mode != UiModeHelper.UiModeType.TV) {
            // If the app is not running in TV mode, start the main activity
            Intent intent = new Intent(this, com.barracoder.android.MainActivity.class);
            startActivity(intent);
            finish();
            return;
        }
        setContentView(R.layout.activity_main_tv);
        getSupportFragmentManager().addOnBackStackChangedListener(
                () -> {
                    Fragment currentFragment = getSupportFragmentManager().findFragmentById(R.id.main_browse_fragment);
                    if (currentFragment instanceof MainFragment && currentFragment.isResumed()) {
                        ((MainFragment) currentFragment).onFragmentStackChanged();
                    }
                }
        );
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Handle key events here
        if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER && event.isLongPress()) {
            MainFragment fragment = (MainFragment) getSupportFragmentManager().findFragmentById(R.id.main_browse_fragment);
            assert fragment != null;
            return fragment.onCenterLongPressed();
        }
        return super.onKeyDown(keyCode, event);
    }
}