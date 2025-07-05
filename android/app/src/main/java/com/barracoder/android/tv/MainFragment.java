package com.barracoder.android.tv;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.core.content.res.ResourcesCompat;
import androidx.leanback.app.BackgroundManager;
import androidx.leanback.app.BrowseSupportFragment;
import androidx.leanback.widget.ArrayObjectAdapter;
import androidx.leanback.widget.HeaderItem;
import androidx.leanback.widget.ListRow;
import androidx.leanback.widget.ListRowPresenter;
import androidx.leanback.widget.OnItemViewClickedListener;
import androidx.leanback.widget.Presenter;
import androidx.leanback.widget.PresenterSelector;
import androidx.leanback.widget.Row;
import androidx.leanback.widget.RowHeaderPresenter;
import androidx.leanback.widget.RowPresenter;
import androidx.core.content.ContextCompat;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.barracoder.android.EmulatorActivity;
import com.barracoder.android.NESItemModel;
import com.barracoder.android.R;
import com.barracoder.android.ROMList;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.util.HashMap;
import java.util.List;

public class MainFragment extends BrowseSupportFragment implements BackgroundProvisioner.BackgroundSetter {
    private static final String TAG = "MainFragment";
    private BackgroundProvisioner mBackgroundProvisioner;
    private final HashMap<String, ArrayObjectAdapter> mRowsAdapterMap = new HashMap<>();
    private BackgroundManager mBackgroundManager;
    private HashMap<String, Integer> mCategoryIcons = new HashMap<>();

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        Log.d(TAG, "onViewCreated: MainFragment view created");
        super.onViewCreated(view, savedInstanceState);
        mBackgroundManager = BackgroundManager.getInstance(requireActivity());
        setupUIElements();
        setupEventListeners();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate: MainFragment created");
        super.onCreate(savedInstanceState);
        loadRows();
    }

    @Override
    public void onStop() {
        super.onStop();
        mBackgroundProvisioner.forgetActivity(this);
        Log.d(TAG, "onStop: MainFragment stopped");
    }

    @Override
    public void onStart() {
        super.onStart();
        mBackgroundProvisioner = BackgroundProvisioner.forActivity(this);
        updateVolatileRows();
        Log.d(TAG, "onStart: MainFragment started");
    }

    private void loadRows() {
        Log.d(TAG, "loadRows: Loading rows");
        ArrayObjectAdapter rowsAdapter = new ArrayObjectAdapter(new ListRowPresenter());
        CardPresenter cardPresenter = new CardPresenter();


        HashMap<String, List<NESItemModel>> categoryMap = ROMList.getCategoryMap(requireActivity());

        int i = 0;
        for (String category : categoryMap.keySet()) {
            List<NESItemModel> list = categoryMap.get(category);
            if (list == null) {
                continue;
            }
            ArrayObjectAdapter listRowAdapter = new ArrayObjectAdapter(cardPresenter);
            mRowsAdapterMap.put(category, listRowAdapter);
            for (NESItemModel item : list) {
                listRowAdapter.add(item);
            }
            HeaderItem header = new HeaderItem(i++, category);
            rowsAdapter.add(new ListRow(header, listRowAdapter));
        }

        setAdapter(rowsAdapter);
    }

    private void updateVolatileRows() {
        if(!ROMList.hasChanged || mRowsAdapterMap.isEmpty())
            return;
        HashMap<String, List<NESItemModel>> categoryMap = ROMList.getVolatileCategoryMap(requireActivity());
        for(String category : categoryMap.keySet()) {
            List<NESItemModel> list = categoryMap.get(category);
            if (list == null) {
                continue;
            }
            ArrayObjectAdapter listRowAdapter = mRowsAdapterMap.get(category);
            if(listRowAdapter == null){
                continue;
            }
            listRowAdapter.setItems(list, null);
            synchronized (listRowAdapter) {
                listRowAdapter.notifyAll();
            }
        }
        ROMList.hasChanged = false;
    }

    public void onFragmentStackChanged() {
        Log.d(TAG, "onFragmentStackChanged: Fragment stack changed");
        updateVolatileRows();
    }

    @SuppressLint("UseCompatLoadingForDrawables")
    private void setupUIElements() {
        setBadgeDrawable(requireActivity().getResources().getDrawable(
                R.mipmap.ic_launcher
        ));
        // setTitle(getString(R.string.browse_title)); // Badge, when set, takes precedent
        // over title
        setHeadersState(HEADERS_ENABLED);
        setHeadersTransitionOnBackEnabled(true);

        // set fastLane (or headers) background color
        setBrandColor(ContextCompat.getColor(requireActivity(), R.color.fastlane_background));
        // set search icon color
        setSearchAffordanceColor(ContextCompat.getColor(requireActivity(), R.color.search_opaque));
        // set the default background
        mCategoryIcons.put("All Games", R.drawable.ic_game);
        mCategoryIcons.put("Recent", R.drawable.ic_time);
        mCategoryIcons.put("Favorites", R.drawable.ic_heart);
        mCategoryIcons.put("Music", R.drawable.ic_music);

        setHeaderPresenterSelector(new PresenterSelector() {
            @Override
            public @Nullable Presenter getPresenter(@Nullable Object item) {
                return new IconHeaderItemPresenter();
            }
        });
    }

    private void setupEventListeners() {
        setOnSearchClickedListener(view -> {
            Intent intent = new Intent(requireActivity(), SearchActivity.class);
            requireActivity().startActivity(intent);
        });

        setOnItemViewClickedListener(new ItemViewClickedListener());
    }

    public boolean onCenterLongPressed() {
        // get the current selected item
        Object obj = getSelectedRowViewHolder().getSelectedItem();
        if(!(obj instanceof NESItemModel)) {
            return false;
        }
        NESItemModel item = (NESItemModel) obj;
        Log.d(TAG, "Selected: " + item);

        ExtraActionFragment.showDetails(this, R.id.main_browse_fragment, item);
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

    public void launchROM(NESItemModel item) {
        ArrayObjectAdapter adapter = mRowsAdapterMap.get("recent");
        if (adapter != null) {
            adapter.add(item);
            adapter.notifyAll();
        }
        EmulatorActivity.launchROM(requireActivity(), item, false, true);
    }

    private final class ItemViewClickedListener implements OnItemViewClickedListener {
        @Override
        public void onItemClicked(Presenter.ViewHolder itemViewHolder, Object item,
                                  RowPresenter.ViewHolder rowViewHolder, Row row) {

            if (item instanceof NESItemModel) {
                launchROM((NESItemModel) item);
            }
        }
    }

    private class IconHeaderItemPresenter extends RowHeaderPresenter {
        private float mUnselectedAlpha;

        @androidx.annotation.NonNull
        @Override
        public ViewHolder onCreateViewHolder(ViewGroup viewGroup) {
            mUnselectedAlpha = viewGroup.getResources()
                    .getFraction(R.fraction.lb_browse_header_unselect_alpha, 1, 1);
            LayoutInflater inflater = (LayoutInflater) viewGroup.getContext()
                    .getSystemService(Context.LAYOUT_INFLATER_SERVICE);

            View view = inflater.inflate(R.layout.tv_row_header, null);
            view.setAlpha(mUnselectedAlpha); // Initialize icons to be at half-opacity.

            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(Presenter.ViewHolder viewHolder, Object item) {
            HeaderItem headerItem = ((ListRow) item).getHeaderItem();
            View rootView = viewHolder.view;
            rootView.setFocusable(true);

            ImageView iconView = rootView.findViewById(R.id.header_icon);
            int drawable = R.drawable.add_ic; // Default icon
            String name = headerItem.getName();
            if(mCategoryIcons.containsKey(name)) {
                Integer dw = mCategoryIcons.get(name);
                if (dw != null) {
                    drawable = dw;
                }
            }

            Drawable icon = ResourcesCompat.getDrawable(getResources(), drawable, null);
            iconView.setImageDrawable(icon);

            TextView label = rootView.findViewById(R.id.header_label);
            label.setText(name);
        }

        @Override
        public void onUnbindViewHolder(@androidx.annotation.NonNull Presenter.ViewHolder viewHolder) {
            // no op
        }

        @Override
        protected void onSelectLevelChanged(RowHeaderPresenter.ViewHolder holder) {
            holder.view.setAlpha(mUnselectedAlpha + holder.getSelectLevel() *
                    (1.0f - mUnselectedAlpha));
        }
    }

}