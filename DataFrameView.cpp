#include "DataFrameView.h"
#include "GroupedDataFrame.h"

GroupedDataFrame DataFrameView::groupby(const std::string colName) const {
	return GroupedDataFrame(*this, name_to_view_idx_.at(colName));
}

DataFrameView DataFrameView::select(std::vector<std::string> column_names) const {
	std::vector<size_t> new_column_indices;
	new_column_indices.reserve(column_names.size());
	for (auto name : column_names) {
		new_column_indices.push_back(column_indices_[getColumnIdx(name)]);
	}
	return DataFrameView(df_, std::move(new_column_indices), row_indices_);
}