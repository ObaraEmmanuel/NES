package com.barracoder.android.tv;

import android.app.Activity;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.leanback.app.BackgroundManager;
import androidx.leanback.app.SearchSupportFragment;
import androidx.leanback.widget.ArrayObjectAdapter;
import androidx.leanback.widget.HeaderItem;
import androidx.leanback.widget.ListRow;
import androidx.leanback.widget.ListRowPresenter;
import androidx.leanback.widget.ObjectAdapter;
import androidx.leanback.widget.OnItemViewClickedListener;
import androidx.leanback.widget.Presenter;
import androidx.leanback.widget.Row;
import androidx.leanback.widget.RowPresenter;

import android.os.Handler;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.barracoder.android.EmulatorActivity;
import com.barracoder.android.NESItemModel;
import com.barracoder.android.R;
import com.barracoder.android.ROMList;

import java.util.List;

/**
 * A simple {@link Fragment} subclass.
 * Use the {@link SearchFragment#newInstance} factory method to
 * create an instance of this fragment.
 */
public class SearchFragment extends SearchSupportFragment
        implements SearchSupportFragment.SearchResultProvider, BackgroundProvisioner.BackgroundSetter
{
    private static final String TAG = "SearchFragment";
    private static final int SEARCH_DELAY_MS = 300;
    private ArrayObjectAdapter mRowsAdapter;
    private final Handler handler = new Handler();
    private SearchRunnable delayedLoad;
    private BackgroundManager mBackgroundManager;
    private BackgroundProvisioner mBackgroundProvisioner;

    private static final String[] SEARCHABLE_CATEGORIES = {
            "All Games",
            "Music",
    };
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mBackgroundManager = BackgroundManager.getInstance(requireActivity());
        mRowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());
        setSearchResultProvider(this);
        setOnItemViewClickedListener(new ItemViewClickedListener());
        delayedLoad = new SearchRunnable();
    }

    @Override
    public void onStop() {
        super.onStop();
        mBackgroundProvisioner.forgetActivity(this);
        Log.d(TAG, "onStop: SearchFragment stopped");
    }

    @Override
    public void onStart() {
        super.onStart();
        mBackgroundProvisioner = BackgroundProvisioner.forActivity(this);
        Log.d(TAG, "onStart: SearchFragment started");
    }

    @Override
    public ObjectAdapter getResultsAdapter() {
        return mRowsAdapter;
    }

    @Override
    public boolean onQueryTextChange(String newQuery) {
        mRowsAdapter.clear();
        if (!TextUtils.isEmpty(newQuery)) {
            delayedLoad.setSearchQuery(newQuery);
            handler.removeCallbacks(delayedLoad);
            handler.postDelayed(delayedLoad, SEARCH_DELAY_MS);
        }
        return true;
    }

    @Override
    public boolean onQueryTextSubmit(String query) {
        mRowsAdapter.clear();
        if (!TextUtils.isEmpty(query)) {
            delayedLoad.setSearchQuery(query);
            handler.removeCallbacks(delayedLoad);
            handler.postDelayed(delayedLoad, SEARCH_DELAY_MS);
        }
        return true;
    }

    @Override
    public Activity getActivityForBackground() {
        return requireActivity();
    }

    @Override
    public BackgroundManager getBackgroundManager() {
        return mBackgroundManager;
    }

    public boolean onCenterLongPressed() {
        // get the current selected item
        int pos = getRowsSupportFragment().getSelectedPosition();
        Object obj = getRowsSupportFragment().getRowViewHolder(pos).getSelectedItem();
        if(!(obj instanceof NESItemModel)) {
            return false;
        }
        NESItemModel item = (NESItemModel) obj;
        Log.d(TAG, "Selected: " + item);

        ExtraActionFragment.showDetails(this, R.id.search_fragment, item);
        return true;
    }

    private class SearchRunnable implements Runnable {
        private String searchQuery;

        public void setSearchQuery(String query) {
            this.searchQuery = query;
        }

        @Override
        public void run() {
            if (searchQuery != null && !searchQuery.isEmpty()) {
                Log.d(TAG, "Searching for: " + searchQuery);
                mRowsAdapter.clear();

                for (String category : SEARCHABLE_CATEGORIES) {
                    ArrayObjectAdapter rowAdapter = new ArrayObjectAdapter(new CardPresenter());
                    HeaderItem header = new HeaderItem(category + " results");
                    List<NESItemModel> results = ROMList.searchByName(searchQuery, category);
                    rowAdapter.addAll(0, results);
                    mRowsAdapter.add(new ListRow(header, rowAdapter));
                }
            }
        }
    }

    private final class ItemViewClickedListener implements OnItemViewClickedListener {
        @Override
        public void onItemClicked(Presenter.ViewHolder itemViewHolder, Object item,
                                  RowPresenter.ViewHolder rowViewHolder, Row row) {

            if (item instanceof NESItemModel){
                NESItemModel nesItem = (NESItemModel) item;
                EmulatorActivity.launchROM(requireActivity(), nesItem, false, true);
            } else if (item instanceof String) {
                Toast.makeText(requireActivity(), ((String) item), Toast.LENGTH_SHORT).show();
            }
        }
    }
}