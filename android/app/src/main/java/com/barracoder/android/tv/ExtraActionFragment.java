package com.barracoder.android.tv;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.barracoder.android.EmulatorActivity;
import com.barracoder.android.NESItemModel;
import com.barracoder.android.R;
import com.barracoder.android.ROMList;
import com.bumptech.glide.Glide;


public class ExtraActionFragment extends Fragment {
    private Button playButton;

    public ExtraActionFragment() {
        // Required empty public constructor
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        NESItemModel item = requireArguments().getParcelable("item");
        assert item != null;
        // Inflate the layout for this fragment
        View view = inflater.inflate(R.layout.fragment_extra_action, container, false);

        ((TextView)view.findViewById(R.id.extraGameText)).setText(item.getName());
        ((TextView)view.findViewById(R.id.extraPublisherText)).setText(getString(R.string.extra_published, item.getPublisher()));
        ((TextView)view.findViewById(R.id.extraDevText)).setText(getString(R.string.extra_developed, item.getDeveloper()));
        ((TextView)view.findViewById(R.id.extraYearText)).setText(String.valueOf(item.getYear()));
        ImageView romImage = view.findViewById(R.id.extraGameImage);
        Glide.with(requireContext())
                .load(item.getImage())
                .placeholder(R.drawable.controller_default)
                .error(R.drawable.controller_default)
                .into(romImage);
        playButton = view.findViewById(R.id.extraPlayBtn);
        if(item.isNSF()) {
            playButton.setText(R.string.listen);
            view.findViewById(R.id.extraGenieBtn).setVisibility(View.GONE);
        } else {
            playButton.setText(R.string.play);
        }
        playButton.requestFocus();
        playButton.setOnClickListener(v -> {
            // launch rom
            EmulatorActivity.launchROM(requireActivity(), item, false, true);
        });

        view.findViewById(R.id.extraGenieBtn).setOnClickListener(v -> {
            // launch genie
            EmulatorActivity.launchROM(requireActivity(), item, true, true);
        });

        Button favButton = view.findViewById(R.id.extraLikeBtn);
        if(ROMList.isInCategory(requireActivity(), item, "favorites")) {
            favButton.setText(R.string.remove_favorite);
        } else {
            favButton.setText(R.string.add_favorite);
        }
        view.findViewById(R.id.extraLikeBtn).setOnClickListener(v -> {
            //add to favorites
            if(ROMList.isInCategory(requireActivity(), item, "favorites")) {
                ROMList.removeFromCategory(requireActivity(), item, "favorites");
                favButton.setText(R.string.add_favorite);
            } else {
                ROMList.addToCategory(requireActivity(), item, "favorites");
                favButton.setText(R.string.remove_favorite);
            }
        });
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        if(playButton != null) {
            playButton.requestFocus();
        }
    }

    public static void showDetails(Fragment fragment, int fragmentId, NESItemModel item) {
        Bundle args = new Bundle();
        args.putParcelable("item", item);
        fragment.requireActivity().getSupportFragmentManager().beginTransaction()
                .add(fragmentId, ExtraActionFragment.class, args)
                .addToBackStack(null)
                .commit();
    }
}