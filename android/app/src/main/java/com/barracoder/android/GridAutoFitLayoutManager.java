package com.barracoder.android;

import android.content.Context;
import android.util.TypedValue;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class GridAutoFitLayoutManager extends GridLayoutManager {
    private int columnWidth;
    private boolean isColumnWidthChanged = true;
    private int lastWidth;
    private int lastHeight;

    private RecyclerView recyclerView;

    public GridAutoFitLayoutManager(@NonNull final Context context, RecyclerView recyclerView, final int columnWidth) {
        /* Initially set spanCount to 1, will be changed automatically later. */
        super(context, 1);
        this.recyclerView = recyclerView;
        setColumnWidth(checkedColumnWidth(context, columnWidth));
    }

    public GridAutoFitLayoutManager(
            @NonNull final Context context,
            final int columnWidth,
            final int orientation,
            final boolean reverseLayout) {

        /* Initially set spanCount to 1, will be changed automatically later. */
        super(context, 1, orientation, reverseLayout);
        setColumnWidth(checkedColumnWidth(context, columnWidth));
    }

    private int checkedColumnWidth(@NonNull final Context context, int columnWidth) {
        if (columnWidth <= 0) {
            /* Set default columnWidth value (48dp here). It is better to move this constant
            to static constant on top, but we need context to convert it to dp, so can't really
            do so. */
            columnWidth = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 180,
                    context.getResources().getDisplayMetrics());
        }
        else
        {
            columnWidth = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, columnWidth,
                    context.getResources().getDisplayMetrics());
        }
        return columnWidth;
    }

    public void setColumnWidth(final int newColumnWidth) {
        if (newColumnWidth > 0 && newColumnWidth != columnWidth) {
            columnWidth = newColumnWidth;
            isColumnWidthChanged = true;
        }
    }

    @Override
    public void onLayoutChildren(@NonNull final RecyclerView.Recycler recycler, @NonNull final RecyclerView.State state) {
        final int width = getWidth();
        final int height = getHeight();
        if (columnWidth > 0 && width > 0 && height > 0 && (isColumnWidthChanged || lastWidth != width || lastHeight != height)) {
            final int totalSpace;
            if (getOrientation() == VERTICAL) {
                totalSpace = width - getPaddingRight() - getPaddingLeft();
            } else {
                totalSpace = height - getPaddingTop() - getPaddingBottom();
            }
            final int spanCount = Math.max(1, totalSpace / columnWidth);
            final int extraSpace = totalSpace - columnWidth * spanCount;
            if(extraSpace > 0)
                recyclerView.setPadding(
                        extraSpace / (spanCount + 1),
                        recyclerView.getPaddingTop(),
                        0,
                        recyclerView.getPaddingBottom());
            setSpanCount(spanCount);
            isColumnWidthChanged = false;
        }
        lastWidth = width;
        lastHeight = height;
        super.onLayoutChildren(recycler, state);
    }
}
