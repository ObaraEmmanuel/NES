package com.barracoder.android.tv;

import android.os.Bundle;
import android.view.KeyEvent;

import androidx.fragment.app.FragmentActivity;

import com.barracoder.android.R;

public class SearchActivity extends FragmentActivity {

    /**
     * Called when the activity is first created.
     */
    private SearchFragment mSearchFragment;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_search);
        if (savedInstanceState == null) {
            mSearchFragment = new SearchFragment();
            getSupportFragmentManager().beginTransaction()
                    .replace(R.id.search_fragment, mSearchFragment)
                    .commitNow();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Handle key events here
        if(keyCode == KeyEvent.KEYCODE_DPAD_CENTER && event.isLongPress()){
            return mSearchFragment.onCenterLongPressed();
        }
        return super.onKeyDown(keyCode, event);
    }
}