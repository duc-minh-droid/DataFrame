#include "DataFrame.h"
#include "DataFrameView.h"
#include "GroupedDataFrame.h"

DataFrameView DataFrame::head(size_t n) const {
	auto row_indices = all_row_indices();
	auto col_indices = all_column_indices();
	size_t k = std::min(n, row_indices.size());
	return DataFrameView(
		this, col_indices,
		std::vector<size_t>(row_indices.begin(), row_indices.begin() + k)
	);
}

DataFrameView DataFrame::select(std::vector<std::string> column_names) const {
	std::vector<size_t> indices;
	for (auto name : column_names) {
		indices.push_back(name_to_index_.at(name));
	}
	return DataFrameView(this, indices, all_row_indices());
}

GroupedDataFrame DataFrame::groupby(const std::string colName) const {
	auto indices = all_column_indices();
	DataFrameView view(this, indices, all_row_indices());
	int key_view_idx = view.getColumnIdx(colName);
	return GroupedDataFrame(view, key_view_idx);
}
